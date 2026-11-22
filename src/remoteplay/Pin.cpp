#include "remoteplay/Pin.h"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace lynx::remoteplay
{

std::optional<std::uint32_t> parsePin(std::string_view text)
{
    const bool onlyDigits = std::ranges::all_of(text, [](char character) {
        return std::isdigit(static_cast<unsigned char>(character)) != 0;
    });

    if (text.size() != kPinDigits || !onlyDigits)
    {
        return std::nullopt;
    }

    std::uint32_t pin = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), pin);

    if (result.ec != std::errc())
    {
        return std::nullopt;
    }

    return pin;
}

} // namespace lynx::remoteplay
