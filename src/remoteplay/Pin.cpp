#include "remoteplay/Pin.h"

#include "core/Text.h"

#include <charconv>

namespace lynx::remoteplay
{

std::optional<std::uint32_t> parsePin(std::string_view text)
{
    if (text.size() != kPinDigits || !core::isDigits(text))
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
