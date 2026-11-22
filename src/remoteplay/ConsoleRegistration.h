#pragma once

#include "remoteplay/AccountId.h"
#include "remoteplay/Console.h"

#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>

namespace lynx::remoteplay
{

// Why pairing failed, as far as chiaki's log tells.
enum class PairingFailure
{
    // No answer at that address: off, asleep or on another network.
    ConsoleNotFound,
    // It answered, but "Vincular dispositivo" isn't open on it.
    NotInLinkScreen,
    WrongPin,
    InvalidAccountId,
    // Refused for another reason, such as too many linked devices.
    Rejected,
    Unknown,
};

struct PairingRequest
{
    std::string host;
    AccountId accountId{};
    std::uint32_t pin = 0;
    // chiaki's ChiakiTarget; 0 when unknown, which pairs as a PS5.
    int target = 0;
};

// What a successful pairing gives this PC.
struct PairedConsole
{
    Credentials credentials;
    std::array<std::uint8_t, kMacSize> serverMac{};
    std::string serverNickname;
    int target = 0;
};

using PairingResult = std::expected<PairedConsole, PairingFailure>;

// chiaki reports only that pairing failed; the reason is in its log lines.
[[nodiscard]] PairingFailure classifyPairingFailure(std::span<const std::string> chiakiLog);

// The host ID discovery reports for the console with this MAC address.
[[nodiscard]] std::string hostIdFromMac(std::span<const std::uint8_t, kMacSize> mac);

// One pairing attempt with chiaki's registration, on its own thread.
class ConsoleRegistration
{
public:
    // Runs once on chiaki's thread, unless the attempt is canceled.
    using Completion = std::function<void(PairingResult result)>;

    [[nodiscard]] static std::expected<std::unique_ptr<ConsoleRegistration>, std::string> start(
        const PairingRequest& request, Completion onDone);

    ConsoleRegistration(const ConsoleRegistration&) = delete;
    ConsoleRegistration& operator=(const ConsoleRegistration&) = delete;
    ConsoleRegistration(ConsoleRegistration&&) = delete;
    ConsoleRegistration& operator=(ConsoleRegistration&&) = delete;
    // Cancels the attempt if it is still running and waits for its thread.
    ~ConsoleRegistration();

private:
    struct State;

    explicit ConsoleRegistration(std::unique_ptr<State> state);

    std::unique_ptr<State> state_;
};

} // namespace lynx::remoteplay
