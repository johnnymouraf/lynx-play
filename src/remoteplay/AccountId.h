#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace mfplay::remoteplay
{

inline constexpr std::size_t kAccountIdSize = 8;

// A PSN account ID as chiaki sends it when pairing: the user ID's 8 bytes,
// little-endian.
using AccountId = std::array<std::uint8_t, kAccountIdSize>;

enum class AccountIdError
{
    Empty,
    // Only digits, but not a 64-bit number above zero.
    InvalidNumber,
    // Neither digits nor Base64 of exactly 8 bytes.
    InvalidFormat,
};

// Reads an account ID typed as the PSN user ID number or as its Base64 form.
[[nodiscard]] std::expected<AccountId, AccountIdError> parseAccountId(std::string_view text);

// The Base64 form, which is how it is saved and shown again.
[[nodiscard]] std::string formatAccountId(const AccountId& accountId);

} // namespace mfplay::remoteplay
