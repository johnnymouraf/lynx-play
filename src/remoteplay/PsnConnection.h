#pragma once

#include "remoteplay/AccountId.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <chiaki/common.h>

namespace mfplay::remoteplay
{

inline constexpr std::size_t kDuidSize = 32;

using DuidBytes = std::array<std::uint8_t, kDuidSize>;

// Where a session reaches the console through the PSN instead of the local
// network.
struct PsnTarget
{
    // The console's PSN device identifier, in hex.
    std::string duid;
    std::string accessToken;
    AccountId accountId{};
};

// The steps of a PSN connection, in the order they run. A registration over
// the PSN stops after `Registering`: the data channel never opens.
enum class ConnectionStage
{
    SigningIn,
    CreatingSession,
    StartingSession,
    PunchingControl,
    Registering,
    PunchingData,
    Connecting,
};

[[nodiscard]] std::string_view stageName(ConnectionStage stage);

// What the interface tells the player about a PSN connection that failed.
enum class PsnFailureKind
{
    // The device identifier isn't one the PSN knows.
    InvalidDevice,
    Canceled,
    Timeout,
    // The PSN has no answer from the console: off, or Remote Play disabled.
    ConsoleOffline,
    Network,
    // The PSN refused: an expired or invalid sign-in.
    Rejected,
    Unknown,
};

struct PsnFailure
{
    ConnectionStage stage = ConnectionStage::SigningIn;
    PsnFailureKind kind = PsnFailureKind::Unknown;
    // chiaki's description of the error; never carries addresses or secrets.
    std::string detail;

    bool operator==(const PsnFailure&) const = default;
};

[[nodiscard]] PsnFailureKind psnFailureKindOf(ChiakiErrorCode error);

// The identifier's 32 bytes out of its 64 hex digits; nothing for any other
// text.
[[nodiscard]] std::optional<DuidBytes> parseDuid(std::string_view hex);

} // namespace mfplay::remoteplay
