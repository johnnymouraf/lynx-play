#include "remoteplay/ConsoleRegistration.h"

#include "core/Text.h"
#include "remoteplay/ChiakiLogSink.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

#include <chiaki/regist.h>

namespace mfplay::remoteplay
{

namespace
{

// Fragments of the lines chiaki's regist.c logs on each kind of failure.
constexpr std::string_view kWrongPinReason = "0x80108b09";
constexpr std::string_view kInvalidAccountReason = "0x80108b02";
constexpr std::string_view kRejectedResponse = "Regist received HTTP code";
constexpr std::string_view kSearchRefused = "refused the registration search";
constexpr std::string_view kTimedOut = "timed out";
constexpr std::string_view kConnectFailed = "connect failed";
constexpr std::string_view kResolveFailed = "getaddrinfo";

PairedConsole toPairedConsole(const ChiakiRegisteredHost& host)
{
    PairedConsole paired;
    std::memcpy(paired.credentials.registKey.data(), host.rp_regist_key, kRegistKeySize);
    std::memcpy(paired.credentials.rpKey.data(), host.rp_key, kRpKeySize);
    paired.credentials.rpKeyType = host.rp_key_type;
    std::memcpy(paired.serverMac.data(), host.server_mac, kMacSize);

    const std::size_t nicknameLength = strnlen(host.server_nickname, sizeof(host.server_nickname));
    paired.serverNickname.assign(host.server_nickname, nicknameLength);
    paired.target = static_cast<int>(host.target);

    return paired;
}

} // namespace

PairingFailure classifyPairingFailure(std::span<const std::string> chiakiLog)
{
    const auto mentions = [chiakiLog](std::string_view fragment) {
        return std::ranges::any_of(chiakiLog, [fragment](const std::string& line) {
            return line.contains(fragment);
        });
    };

    if (mentions(kWrongPinReason))
    {
        return PairingFailure::WrongPin;
    }

    if (mentions(kInvalidAccountReason))
    {
        return PairingFailure::InvalidAccountId;
    }

    if (mentions(kRejectedResponse))
    {
        return PairingFailure::Rejected;
    }

    if (mentions(kSearchRefused))
    {
        return PairingFailure::NotInLinkScreen;
    }

    if (mentions(kTimedOut) || mentions(kConnectFailed) || mentions(kResolveFailed))
    {
        return PairingFailure::ConsoleNotFound;
    }

    return PairingFailure::Unknown;
}

std::string hostIdFromMac(std::span<const std::uint8_t, kMacSize> mac)
{
    return core::toHex(mac);
}

struct ConsoleRegistration::State
{
    Completion onDone;
    std::mutex logMutex;
    // Guarded by logMutex.
    std::vector<std::string> log;
    ChiakiLogSink logSink{[this](ChiakiLogLevel, std::string_view message) { remember(message); }};
    ChiakiRegist regist{};
    bool started = false;

    void remember(std::string_view message);
    [[nodiscard]] PairingFailure failure();
    static void onEvent(ChiakiRegistEvent* event, void* user);
};

void ConsoleRegistration::State::remember(std::string_view message)
{
    const std::scoped_lock lock(logMutex);
    log.emplace_back(message);
}

PairingFailure ConsoleRegistration::State::failure()
{
    const std::scoped_lock lock(logMutex);
    return classifyPairingFailure(log);
}

void ConsoleRegistration::State::onEvent(ChiakiRegistEvent* event, void* user)
{
    auto& state = *static_cast<State*>(user);

    switch (event->type)
    {
    case CHIAKI_REGIST_EVENT_TYPE_FINISHED_SUCCESS:
        state.onDone(toPairedConsole(*event->registered_host));
        return;
    case CHIAKI_REGIST_EVENT_TYPE_FINISHED_FAILED:
        state.onDone(std::unexpected(state.failure()));
        return;
    case CHIAKI_REGIST_EVENT_TYPE_FINISHED_CANCELED:
        return;
    }
}

std::expected<std::unique_ptr<ConsoleRegistration>, std::string> ConsoleRegistration::start(
    const PairingRequest& request, Completion onDone)
{
    auto state = std::make_unique<State>();
    state->onDone = std::move(onDone);

    ChiakiRegistInfo info{};
    info.target = static_cast<ChiakiTarget>(request.target != 0 ? request.target
                                                                 : CHIAKI_TARGET_PS5_1);
    info.host = request.host.c_str();
    info.broadcast = false;
    info.psn_online_id = nullptr;
    std::ranges::copy(request.accountId, info.psn_account_id);
    info.pin = request.pin;

    const ChiakiErrorCode result = chiaki_regist_start(&state->regist, state->logSink.log(),
                                                       &info, &State::onEvent, state.get());

    if (result != CHIAKI_ERR_SUCCESS)
    {
        return std::unexpected(std::string("chiaki_regist_start failed: ") +
                               chiaki_error_string(result));
    }

    state->started = true;
    return std::unique_ptr<ConsoleRegistration>(new ConsoleRegistration(std::move(state)));
}

ConsoleRegistration::ConsoleRegistration(std::unique_ptr<State> state) : state_(std::move(state))
{
}

ConsoleRegistration::~ConsoleRegistration()
{
    if (!state_->started)
    {
        return;
    }

    chiaki_regist_stop(&state_->regist);
    chiaki_regist_fini(&state_->regist);
}

} // namespace mfplay::remoteplay
