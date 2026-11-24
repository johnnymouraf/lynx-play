#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace mfplay::remoteplay
{

enum class HostAddressError
{
    Empty,
    // Looks like an IPv4 address, but a part is above 255.
    Ipv4PartOutOfRange,
    // Neither an IPv4 address nor a host name.
    Invalid,
};

// Checks an address typed in "Adicionar pelo IP": an IPv4 address or a host
// name. Gives it back trimmed, with a host name in lowercase.
[[nodiscard]] std::expected<std::string, HostAddressError> normalizeHostAddress(
    std::string_view text);

} // namespace mfplay::remoteplay
