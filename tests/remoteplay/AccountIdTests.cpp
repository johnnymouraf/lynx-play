#include "remoteplay/AccountId.h"

#include <catch2/catch_test_macros.hpp>

using mfplay::remoteplay::AccountId;
using mfplay::remoteplay::AccountIdError;
using mfplay::remoteplay::formatAccountId;
using mfplay::remoteplay::parseAccountId;

namespace
{

constexpr AccountId kSmallId{0x02, 0x01, 0, 0, 0, 0, 0, 0};
constexpr AccountId kLargestId{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

} // namespace

TEST_CASE("parseAccountId reads the user ID number as 8 little-endian bytes")
{
    CHECK(parseAccountId("258") == kSmallId);
    CHECK(parseAccountId("18446744073709551615") == kLargestId);
    CHECK(parseAccountId("  258 ") == kSmallId);
}

TEST_CASE("parseAccountId reads the Base64 form, with or without padding")
{
    CHECK(parseAccountId("AgEAAAAAAAA=") == kSmallId);
    CHECK(parseAccountId("AgEAAAAAAAA") == kSmallId);
    CHECK(parseAccountId("//////////8=") == kLargestId);
}

TEST_CASE("parseAccountId rejects what can't be an account ID")
{
    CHECK(parseAccountId("") == std::unexpected(AccountIdError::Empty));
    CHECK(parseAccountId("   ") == std::unexpected(AccountIdError::Empty));
    CHECK(parseAccountId("0") == std::unexpected(AccountIdError::InvalidNumber));
    CHECK(parseAccountId("18446744073709551616") ==
          std::unexpected(AccountIdError::InvalidNumber));
    CHECK(parseAccountId("Kx3f") == std::unexpected(AccountIdError::InvalidFormat));
    CHECK(parseAccountId("AgEAAAAAAAAA") == std::unexpected(AccountIdError::InvalidFormat));
    CHECK(parseAccountId("not an id!") == std::unexpected(AccountIdError::InvalidFormat));
}

TEST_CASE("formatAccountId gives the Base64 form back")
{
    CHECK(formatAccountId(kSmallId) == "AgEAAAAAAAA=");
}
