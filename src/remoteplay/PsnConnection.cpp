#include "remoteplay/PsnConnection.h"

#include <charconv>

namespace mfplay::remoteplay
{

namespace
{

constexpr int kHexBase = 16;

std::optional<std::uint8_t> byteOf(std::string_view pair)
{
    std::uint8_t value = 0;
    const char* end = pair.data() + pair.size();
    const auto [stop, error] = std::from_chars(pair.data(), end, value, kHexBase);

    if (error != std::errc{} || stop != end)
    {
        return std::nullopt;
    }

    return value;
}

} // namespace

std::string_view stageName(ConnectionStage stage)
{
    switch (stage)
    {
    case ConnectionStage::SigningIn:
        return "signing in";
    case ConnectionStage::CreatingSession:
        return "creating the session";
    case ConnectionStage::StartingSession:
        return "starting the session";
    case ConnectionStage::PunchingControl:
        return "opening the control channel";
    case ConnectionStage::Registering:
        return "registering the console";
    case ConnectionStage::PunchingData:
        return "opening the data channel";
    case ConnectionStage::Connecting:
        return "connecting";
    }

    return "unknown";
}

PsnFailureKind psnFailureKindOf(ChiakiErrorCode error)
{
    switch (error)
    {
    case CHIAKI_ERR_INVALID_DATA:
    case CHIAKI_ERR_PARSE_ADDR:
        return PsnFailureKind::InvalidDevice;
    case CHIAKI_ERR_CANCELED:
        return PsnFailureKind::Canceled;
    case CHIAKI_ERR_TIMEOUT:
        return PsnFailureKind::Timeout;
    case CHIAKI_ERR_HOST_DOWN:
        return PsnFailureKind::ConsoleOffline;
    case CHIAKI_ERR_NETWORK:
    case CHIAKI_ERR_HOST_UNREACH:
    case CHIAKI_ERR_CONNECTION_REFUSED:
    case CHIAKI_ERR_DISCONNECTED:
        return PsnFailureKind::Network;
    case CHIAKI_ERR_HTTP_NONOK:
    case CHIAKI_ERR_INVALID_RESPONSE:
        return PsnFailureKind::Rejected;
    default:
        return PsnFailureKind::Unknown;
    }
}

std::optional<DuidBytes> parseDuid(std::string_view hex)
{
    if (hex.size() != kDuidSize * 2)
    {
        return std::nullopt;
    }

    DuidBytes bytes{};

    for (std::size_t index = 0; index < bytes.size(); ++index)
    {
        const std::optional<std::uint8_t> value = byteOf(hex.substr(index * 2, 2));

        if (!value)
        {
            return std::nullopt;
        }

        bytes[index] = *value;
    }

    return bytes;
}

} // namespace mfplay::remoteplay
