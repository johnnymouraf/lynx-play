#include "remoteplay/Base64.h"

namespace mfplay::remoteplay
{

namespace
{

constexpr std::string_view kAlphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr char kPadding = '=';
constexpr std::size_t kGroupBytes = 3;
constexpr std::size_t kGroupChars = 4;
constexpr std::size_t kMaxPadding = 2;
constexpr std::uint32_t kSextetMask = 0x3FU;

std::optional<std::uint32_t> sextet(char character)
{
    const std::size_t position = kAlphabet.find(character);

    if (position == std::string_view::npos)
    {
        return std::nullopt;
    }

    return static_cast<std::uint32_t>(position);
}

} // namespace

std::string encodeBase64(std::span<const std::uint8_t> bytes)
{
    std::string text;
    text.reserve((bytes.size() + kGroupBytes - 1) / kGroupBytes * kGroupChars);

    for (std::size_t i = 0; i < bytes.size(); i += kGroupBytes)
    {
        const std::size_t remaining = bytes.size() - i;
        std::uint32_t group = static_cast<std::uint32_t>(bytes[i]) << 16U;

        if (remaining > 1)
        {
            group |= static_cast<std::uint32_t>(bytes[i + 1]) << 8U;
        }

        if (remaining > 2)
        {
            group |= bytes[i + 2];
        }

        text += kAlphabet[(group >> 18U) & kSextetMask];
        text += kAlphabet[(group >> 12U) & kSextetMask];
        text += remaining > 1 ? kAlphabet[(group >> 6U) & kSextetMask] : kPadding;
        text += remaining > 2 ? kAlphabet[group & kSextetMask] : kPadding;
    }

    return text;
}

std::optional<std::vector<std::uint8_t>> decodeBase64(std::string_view text)
{
    std::size_t padding = 0;

    while (padding < kMaxPadding && text.ends_with(kPadding))
    {
        text.remove_suffix(1);
        ++padding;
    }

    const bool paddedLengthIsWrong = padding > 0 && (text.size() + padding) % kGroupChars != 0;

    if (paddedLengthIsWrong || text.size() % kGroupChars == 1)
    {
        return std::nullopt;
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() * kGroupBytes / kGroupChars);

    std::uint32_t buffer = 0;
    int bufferedBits = 0;

    for (const char character : text)
    {
        const auto value = sextet(character);

        if (!value)
        {
            return std::nullopt;
        }

        buffer = ((buffer << 6U) | *value) & 0xFFFFFFU;
        bufferedBits += 6;

        if (bufferedBits >= 8)
        {
            bufferedBits -= 8;
            bytes.push_back(static_cast<std::uint8_t>((buffer >> bufferedBits) & 0xFFU));
        }
    }

    return bytes;
}

} // namespace mfplay::remoteplay
