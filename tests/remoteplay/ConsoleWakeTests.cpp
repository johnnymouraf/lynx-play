#include "remoteplay/ConsoleWake.h"

#include <catch2/catch_test_macros.hpp>

using lynx::remoteplay::kRegistKeySize;
using lynx::remoteplay::wakeCredential;

namespace
{

using RegistKey = std::array<std::uint8_t, kRegistKeySize>;

} // namespace

TEST_CASE("wakeCredential reads the registration key's hex digits as a number")
{
    const RegistKey key{'1', '2', '3', '4', 'a', 'b', 'c', 'd'};

    CHECK(wakeCredential(key) == 0x1234abcdU);
}

TEST_CASE("wakeCredential rejects a key without hex digits")
{
    const RegistKey empty{};
    const RegistKey notHex{'1', '2', 'x', 'z'};

    CHECK_FALSE(wakeCredential(empty).has_value());
    CHECK_FALSE(wakeCredential(notHex).has_value());
}
