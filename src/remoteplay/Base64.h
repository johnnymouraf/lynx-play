#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lynx::remoteplay
{

// Standard Base64 (RFC 4648 alphabet), with padding.
[[nodiscard]] std::string encodeBase64(std::span<const std::uint8_t> bytes);

// Accepts the text with or without its padding; nullopt for any other
// character or an impossible length.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> decodeBase64(std::string_view text);

} // namespace lynx::remoteplay
