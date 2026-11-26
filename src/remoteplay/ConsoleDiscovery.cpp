#include "remoteplay/ConsoleDiscovery.h"

#include "remoteplay/ChiakiLogSink.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <mutex>
#include <ranges>
#include <utility>

#include <chiaki/discoveryservice.h>
#include <spdlog/spdlog.h>

namespace mfplay::remoteplay
{

namespace
{

constexpr std::uint64_t kPingIntervalMs = 1000;
constexpr std::uint64_t kDropAfterMissedPings = 3;
constexpr std::size_t kMaxBroadcastHosts = 16;
#if defined(_WIN32)
constexpr ULONG kInitialAdapterBufferSize = 16 * 1024;
constexpr int kAdapterQueryAttempts = 3;
constexpr std::uint8_t kMaxBroadcastPrefixLength = 30;
#endif

std::string text(const char* value)
{
    return value != nullptr ? std::string(value) : std::string();
}

std::string uppercase(std::string value)
{
    std::ranges::transform(value, value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

HostState toHostState(ChiakiDiscoveryHostState state)
{
    switch (state)
    {
    case CHIAKI_DISCOVERY_HOST_STATE_READY:
        return HostState::Ready;
    case CHIAKI_DISCOVERY_HOST_STATE_STANDBY:
        return HostState::Standby;
    case CHIAKI_DISCOVERY_HOST_STATE_UNKNOWN:
        return HostState::Unknown;
    }

    return HostState::Unknown;
}

DiscoveredHost toDiscoveredHost(ChiakiDiscoveryHost& host, const std::string& queriedHost)
{
    DiscoveredHost result{
        .hostId = uppercase(text(host.host_id)),
        .name = text(host.host_name),
        .address = text(host.host_addr),
        .state = toHostState(host.state),
        .ps5 = chiaki_discovery_host_is_ps5(&host),
        .systemVersion = text(host.system_version),
        .target = 0,
        .queriedHost = queriedHost,
    };

    result.target = result.ps5 ? CHIAKI_TARGET_PS5_UNKNOWN : CHIAKI_TARGET_PS4_UNKNOWN;

    if (host.system_version != nullptr)
    {
        result.target = static_cast<int>(chiaki_discovery_host_system_version_target(&host));
    }

    return result;
}

#if defined(_WIN32)

std::vector<std::byte> queryAdapters()
{
    ULONG size = kInitialAdapterBufferSize;
    constexpr ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                            GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_FRIENDLY_NAME;

    for (int attempt = 0; attempt < kAdapterQueryAttempts; ++attempt)
    {
        std::vector<std::byte> buffer(size);
        auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        const ULONG result = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &size);

        if (result == NO_ERROR)
        {
            return buffer;
        }

        if (result != ERROR_BUFFER_OVERFLOW)
        {
            spdlog::warn("GetAdaptersAddresses failed: {}", result);
            return {};
        }
    }

    return {};
}

// The directed broadcast address of every IPv4 network on an adapter that is
// up, so the search reaches consoles that ignore 255.255.255.255.
std::vector<sockaddr_storage> broadcastAddresses()
{
    std::vector<std::byte> buffer = queryAdapters();
    std::vector<sockaddr_storage> addresses;

    if (buffer.empty())
    {
        return addresses;
    }

    auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

    for (; adapter != nullptr; adapter = adapter->Next)
    {
        const bool usable = adapter->OperStatus == IfOperStatusUp &&
                            adapter->IfType != IF_TYPE_SOFTWARE_LOOPBACK;

        if (!usable)
        {
            continue;
        }

        for (auto* unicast = adapter->FirstUnicastAddress; unicast != nullptr;
             unicast = unicast->Next)
        {
            if (unicast->Address.lpSockaddr->sa_family != AF_INET ||
                unicast->OnLinkPrefixLength > kMaxBroadcastPrefixLength)
            {
                continue;
            }

            sockaddr_in address{};
            std::memcpy(&address, unicast->Address.lpSockaddr, sizeof(address));

            const ULONG hostBits = 32U - unicast->OnLinkPrefixLength;
            const ULONG hostMask = (1UL << hostBits) - 1UL;
            address.sin_addr.s_addr |= htonl(hostMask);
            address.sin_port = 0;

            // Ethernet and Wi-Fi on the same network share its broadcast address.
            const bool listed = std::ranges::any_of(addresses, [&address](const auto& stored) {
                const auto& known = reinterpret_cast<const sockaddr_in&>(stored);
                return known.sin_addr.s_addr == address.sin_addr.s_addr;
            });

            if (listed)
            {
                continue;
            }

            sockaddr_storage storage{};
            std::memcpy(&storage, &address, sizeof(address));
            addresses.push_back(storage);
        }
    }

    return addresses;
}

#else

std::vector<sockaddr_storage> broadcastAddresses()
{
    return {};
}

#endif

} // namespace

struct ConsoleDiscovery::State
{
    // One chiaki discovery service and the consoles it last reported.
    struct Service
    {
        State* owner = nullptr;
        std::string queriedHost;
        ChiakiDiscoveryService chiaki{};
        bool running = false;
        // Guarded by owner->mutex.
        std::vector<DiscoveredHost> hosts;
    };

    HostsCallback onHosts;
    ChiakiLogSink logSink;
    bool hasNetwork = false;

    // Guards the service list and each service's hosts. Never held while
    // stopping a service: its thread may be waiting for it.
    std::mutex mutex;
    std::unique_ptr<Service> broadcast;
    std::map<std::string, std::unique_ptr<Service>> manual;

    static void onServiceHosts(ChiakiDiscoveryHost* hosts, std::size_t count, void* user);
    [[nodiscard]] std::expected<void, std::string> startBroadcast(Service& service);
    [[nodiscard]] std::expected<void, std::string> startManual(Service& service);
    [[nodiscard]] ChiakiDiscoveryServiceOptions baseOptions(Service& service) const;
    void publish();
};

void ConsoleDiscovery::State::onServiceHosts(ChiakiDiscoveryHost* hosts, std::size_t count,
                                             void* user)
{
    auto& service = *static_cast<Service*>(user);
    std::vector<DiscoveredHost> converted;
    converted.reserve(count);

    for (std::size_t i = 0; i < count; ++i)
    {
        converted.push_back(toDiscoveredHost(hosts[i], service.queriedHost));
    }

    {
        const std::scoped_lock lock(service.owner->mutex);
        service.hosts = std::move(converted);
    }

    service.owner->publish();
}

ChiakiDiscoveryServiceOptions ConsoleDiscovery::State::baseOptions(Service& service) const
{
    ChiakiDiscoveryServiceOptions options{};
    options.ping_ms = kPingIntervalMs;
    options.ping_initial_ms = 0;
    options.host_drop_pings = kDropAfterMissedPings;
    options.cb = &State::onServiceHosts;
    options.cb_user = &service;
    return options;
}

std::expected<void, std::string> ConsoleDiscovery::State::startBroadcast(Service& service)
{
    std::vector<sockaddr_storage> networks = broadcastAddresses();
    hasNetwork = !networks.empty();

    sockaddr_in everyone{};
    everyone.sin_family = AF_INET;
    everyone.sin_addr.s_addr = INADDR_BROADCAST;

    sockaddr_storage sendAddress{};
    std::memcpy(&sendAddress, &everyone, sizeof(everyone));

    ChiakiDiscoveryServiceOptions options = baseOptions(service);
    options.hosts_max = kMaxBroadcastHosts;
    options.send_addr = &sendAddress;
    options.send_addr_size = sizeof(everyone);
    options.broadcast_addrs = networks.data();
    options.broadcast_num = networks.size();

    const ChiakiErrorCode result = chiaki_discovery_service_init(&service.chiaki, &options,
                                                                 logSink.log());

    if (result != CHIAKI_ERR_SUCCESS)
    {
        return std::unexpected(std::string("chiaki_discovery_service_init failed: ") +
                               chiaki_error_string(result));
    }

    service.running = true;
    spdlog::info("Console search started on {} network(s)", networks.size());
    return {};
}

std::expected<void, std::string> ConsoleDiscovery::State::startManual(Service& service)
{
    sockaddr_storage sendAddress{};
    sendAddress.ss_family = AF_INET;

    ChiakiDiscoveryServiceOptions options = baseOptions(service);
    options.hosts_max = 1;
    options.send_addr = &sendAddress;
    options.send_addr_size = sizeof(sockaddr_in);
    options.send_host = service.queriedHost.data();

    const ChiakiErrorCode result = chiaki_discovery_service_init(&service.chiaki, &options,
                                                                 logSink.log());

    if (result != CHIAKI_ERR_SUCCESS)
    {
        return std::unexpected(std::string("chiaki_discovery_service_init failed: ") +
                               chiaki_error_string(result));
    }

    service.running = true;
    spdlog::info("Querying {} directly", service.queriedHost);
    return {};
}

// Holds the mutex while calling onHosts, so the lists reach it in order.
void ConsoleDiscovery::State::publish()
{
    const std::scoped_lock lock(mutex);
    std::vector<DiscoveredHost> all;

    if (broadcast != nullptr)
    {
        all.append_range(broadcast->hosts);
    }

    for (const auto& service : manual | std::views::values)
    {
        all.append_range(service->hosts);
    }

    onHosts(std::move(all));
}

std::expected<std::unique_ptr<ConsoleDiscovery>, std::string> ConsoleDiscovery::start(
    HostsCallback onHosts)
{
    auto state = std::make_unique<State>();
    state->onHosts = std::move(onHosts);
    state->broadcast = std::make_unique<State::Service>();
    state->broadcast->owner = state.get();

    if (auto started = state->startBroadcast(*state->broadcast); !started)
    {
        return std::unexpected(started.error());
    }

    return std::unique_ptr<ConsoleDiscovery>(new ConsoleDiscovery(std::move(state)));
}

ConsoleDiscovery::ConsoleDiscovery(std::unique_ptr<State> state) : state_(std::move(state))
{
}

ConsoleDiscovery::~ConsoleDiscovery()
{
    std::vector<std::unique_ptr<State::Service>> services;

    {
        const std::scoped_lock lock(state_->mutex);
        services.push_back(std::move(state_->broadcast));

        for (auto& service : state_->manual | std::views::values)
        {
            services.push_back(std::move(service));
        }

        state_->manual.clear();
    }

    for (const auto& service : services)
    {
        if (service != nullptr && service->running)
        {
            chiaki_discovery_service_fini(&service->chiaki);
        }
    }
}

bool ConsoleDiscovery::hasNetwork() const
{
    return state_->hasNetwork;
}

void ConsoleDiscovery::setManualHosts(const std::vector<std::string>& hosts)
{
    std::vector<std::unique_ptr<State::Service>> stopped;
    std::vector<State::Service*> added;

    {
        const std::scoped_lock lock(state_->mutex);

        for (auto entry = state_->manual.begin(); entry != state_->manual.end();)
        {
            if (std::ranges::contains(hosts, entry->first))
            {
                ++entry;
                continue;
            }

            stopped.push_back(std::move(entry->second));
            entry = state_->manual.erase(entry);
        }

        for (const std::string& host : hosts)
        {
            if (state_->manual.contains(host))
            {
                continue;
            }

            auto service = std::make_unique<State::Service>();
            service->owner = state_.get();
            service->queriedHost = host;
            added.push_back(service.get());
            state_->manual.emplace(host, std::move(service));
        }
    }

    if (stopped.empty() && added.empty())
    {
        return;
    }

    for (const auto& service : stopped)
    {
        if (service->running)
        {
            chiaki_discovery_service_fini(&service->chiaki);
        }
    }

    for (State::Service* service : added)
    {
        if (auto started = state_->startManual(*service); !started)
        {
            const std::string host = service->queriedHost;
            spdlog::error("Cannot query {}: {}", host, started.error());

            const std::scoped_lock lock(state_->mutex);
            state_->manual.erase(host);
        }
    }

    state_->publish();
}

} // namespace mfplay::remoteplay
