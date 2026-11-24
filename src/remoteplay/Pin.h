#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace mfplay::remoteplay
{

inline constexpr std::size_t kPinDigits = 8;

// The code the console shows on "Vincular dispositivo": exactly 8 digits.
[[nodiscard]] std::optional<std::uint32_t> parsePin(std::string_view text);

} // namespace mfplay::remoteplay
