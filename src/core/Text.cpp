#include "core/Text.h"

#include <algorithm>
#include <cctype>

namespace mfplay::core
{

namespace
{

bool isSpace(char character)
{
    return std::isspace(static_cast<unsigned char>(character)) != 0;
}

} // namespace

std::string_view trimmed(std::string_view text)
{
    while (!text.empty() && isSpace(text.front()))
    {
        text.remove_prefix(1);
    }

    while (!text.empty() && isSpace(text.back()))
    {
        text.remove_suffix(1);
    }

    return text;
}

bool isDigits(std::string_view text)
{
    const auto isDigit = [](char character) { return character >= '0' && character <= '9'; };

    return !text.empty() && std::ranges::all_of(text, isDigit);
}

std::string toHex(std::span<const std::uint8_t> bytes)
{
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string text;
    text.reserve(bytes.size() * 2);

    for (const std::uint8_t byte : bytes)
    {
        text += digits[byte >> 4U];
        text += digits[byte & 0x0FU];
    }

    return text;
}

} // namespace mfplay::core
