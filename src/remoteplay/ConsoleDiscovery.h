#pragma once

#include "remoteplay/Console.h"

#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lynx::remoteplay
{

// Finds PlayStation consoles: a broadcast search on every IPv4 network the PC
// is on, plus direct queries to addresses the user typed in.
class ConsoleDiscovery
{
public:
    // Gets every console that currently answers, each time that list changes,
    // on chiaki's discovery threads.
    using HostsCallback = std::function<void(std::vector<DiscoveredHost> hosts)>;

    [[nodiscard]] static std::expected<std::unique_ptr<ConsoleDiscovery>, std::string> start(
        HostsCallback onHosts);

    ConsoleDiscovery(const ConsoleDiscovery&) = delete;
    ConsoleDiscovery& operator=(const ConsoleDiscovery&) = delete;
    ConsoleDiscovery(ConsoleDiscovery&&) = delete;
    ConsoleDiscovery& operator=(ConsoleDiscovery&&) = delete;
    ~ConsoleDiscovery();

    // False when no network adapter had an IPv4 address as the search began.
    [[nodiscard]] bool hasNetwork() const;

    // Queries these addresses (IPv4 or host names) directly from now on, and
    // stops querying the ones left out.
    void setManualHosts(const std::vector<std::string>& hosts);

private:
    struct State;

    explicit ConsoleDiscovery(std::unique_ptr<State> state);

    std::unique_ptr<State> state_;
};

} // namespace lynx::remoteplay
