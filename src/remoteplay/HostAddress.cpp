#include "remoteplay/HostAddress.h"

#include "core/Text.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <ranges>
#include <vector>

namespace mfplay::remoteplay
{

namespace
{

constexpr std::size_t kIpv4Parts = 4;
constexpr std::size_t kMaxIpv4PartDigits = 3;
constexpr int kMaxIpv4PartValue = 255;
constexpr std::size_t kMaxHostNameLength = 253;
constexpr std::size_t kMaxLabelLength = 63;

std::vector<std::string_view> split(std::string_view text, char separator)
{
    std::vector<std::string_view> parts;

    for (const auto part : std::views::split(text, separator))
    {
        parts.emplace_back(part.begin(), part.end());
    }

    return parts;
}

bool looksLikeIpv4(std::string_view text)
{
    return std::ranges::all_of(text, [](char character) {
        return character == '.' || (character >= '0' && character <= '9');
    });
}

std::expected<void, HostAddressError> checkIpv4(std::string_view text)
{
    const std::vector<std::string_view> parts = split(text, '.');

    if (parts.size() != kIpv4Parts)
    {
        return std::unexpected(HostAddressError::Invalid);
    }

    bool outOfRange = false;

    for (const std::string_view part : parts)
    {
        if (!core::isDigits(part) || part.size() > kMaxIpv4PartDigits)
        {
            return std::unexpected(HostAddressError::Invalid);
        }

        int value = 0;
        const auto result = std::from_chars(part.data(), part.data() + part.size(), value);

        if (result.ec != std::errc())
        {
            return std::unexpected(HostAddressError::Invalid);
        }

        outOfRange = outOfRange || value > kMaxIpv4PartValue;
    }

    if (outOfRange)
    {
        return std::unexpected(HostAddressError::Ipv4PartOutOfRange);
    }

    return {};
}

bool isHostNameLabel(std::string_view label)
{
    const bool validCharacters = std::ranges::all_of(label, [](char character) {
        return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '-';
    });

    return !label.empty() && label.size() <= kMaxLabelLength && validCharacters &&
           label.front() != '-' && label.back() != '-';
}

bool isHostName(std::string_view text)
{
    const std::vector<std::string_view> labels = split(text, '.');

    return text.size() <= kMaxHostNameLength && std::ranges::all_of(labels, isHostNameLabel);
}

std::string lowercase(std::string_view text)
{
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

} // namespace

std::expected<std::string, HostAddressError> normalizeHostAddress(std::string_view text)
{
    const std::string_view address = core::trimmed(text);

    if (address.empty())
    {
        return std::unexpected(HostAddressError::Empty);
    }

    if (looksLikeIpv4(address))
    {
        if (auto valid = checkIpv4(address); !valid)
        {
            return std::unexpected(valid.error());
        }

        return std::string(address);
    }

    if (!isHostName(address))
    {
        return std::unexpected(HostAddressError::Invalid);
    }

    return lowercase(address);
}

} // namespace mfplay::remoteplay
