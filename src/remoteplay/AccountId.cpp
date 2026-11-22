#include "remoteplay/AccountId.h"

#include "remoteplay/Base64.h"

#include <algorithm>
#include <cctype>
#include <charconv>

namespace lynx::remoteplay
{

namespace
{

std::string_view trim(std::string_view text)
{
    const auto isSpace = [](unsigned char character) { return std::isspace(character) != 0; };

    while (!text.empty() && isSpace(static_cast<unsigned char>(text.front())))
    {
        text.remove_prefix(1);
    }

    while (!text.empty() && isSpace(static_cast<unsigned char>(text.back())))
    {
        text.remove_suffix(1);
    }

    return text;
}

bool isAllDigits(std::string_view text)
{
    return std::ranges::all_of(text, [](char character) {
        return std::isdigit(static_cast<unsigned char>(character)) != 0;
    });
}

std::expected<AccountId, AccountIdError> fromNumber(std::string_view digits)
{
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value);

    if (error != std::errc() || end != digits.data() + digits.size() || value == 0)
    {
        return std::unexpected(AccountIdError::InvalidNumber);
    }

    AccountId accountId{};

    for (std::size_t i = 0; i < kAccountIdSize; ++i)
    {
        accountId[i] = static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU);
    }

    return accountId;
}

std::expected<AccountId, AccountIdError> fromBase64(std::string_view text)
{
    const auto bytes = decodeBase64(text);

    if (!bytes || bytes->size() != kAccountIdSize)
    {
        return std::unexpected(AccountIdError::InvalidFormat);
    }

    AccountId accountId{};
    std::ranges::copy(*bytes, accountId.begin());
    return accountId;
}

} // namespace

std::expected<AccountId, AccountIdError> parseAccountId(std::string_view text)
{
    const std::string_view trimmed = trim(text);

    if (trimmed.empty())
    {
        return std::unexpected(AccountIdError::Empty);
    }

    if (isAllDigits(trimmed))
    {
        return fromNumber(trimmed);
    }

    return fromBase64(trimmed);
}

std::string formatAccountId(const AccountId& accountId)
{
    return encodeBase64(accountId);
}

} // namespace lynx::remoteplay
