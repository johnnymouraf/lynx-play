#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace mfplay::core
{

// The text without the ASCII whitespace at its ends.
[[nodiscard]] std::string_view trimmed(std::string_view text);

// True when the text is not empty and holds only the digits 0 to 9.
[[nodiscard]] bool isDigits(std::string_view text);

// Uppercase hexadecimal, two digits per byte.
[[nodiscard]] std::string toHex(std::span<const std::uint8_t> bytes);

} // namespace mfplay::core
