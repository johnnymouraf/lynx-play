#include "remoteplay/Pin.h"

#include <catch2/catch_test_macros.hpp>

using mfplay::remoteplay::parsePin;

TEST_CASE("parsePin accepts exactly 8 digits")
{
    CHECK(parsePin("48152906") == 48152906U);
    CHECK(parsePin("00000001") == 1U);
}

TEST_CASE("parsePin rejects anything but 8 digits")
{
    CHECK_FALSE(parsePin("").has_value());
    CHECK_FALSE(parsePin("4815290").has_value());
    CHECK_FALSE(parsePin("481529061").has_value());
    CHECK_FALSE(parsePin("4815-290").has_value());
    CHECK_FALSE(parsePin(" 4815290").has_value());
    CHECK_FALSE(parsePin("abcdefgh").has_value());
}
