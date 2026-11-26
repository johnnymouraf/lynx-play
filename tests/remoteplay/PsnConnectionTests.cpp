#include "remoteplay/PsnConnection.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace mfplay::remoteplay;

TEST_CASE("A device identifier of 64 hex digits becomes its 32 bytes")
{
    const std::string hex = "00ff10" + std::string(58, 'A');
    const auto bytes = parseDuid(hex);

    REQUIRE(bytes.has_value());
    CHECK((*bytes)[0] == 0x00);
    CHECK((*bytes)[1] == 0xff);
    CHECK((*bytes)[2] == 0x10);
    CHECK((*bytes)[31] == 0xAA);
}

TEST_CASE("An identifier that isn't 64 hex digits is rejected")
{
    CHECK_FALSE(parseDuid("").has_value());
    CHECK_FALSE(parseDuid(std::string(62, '0')).has_value());
    CHECK_FALSE(parseDuid(std::string(66, '0')).has_value());
    CHECK_FALSE(parseDuid(std::string(63, '0') + "g").has_value());
    CHECK_FALSE(parseDuid(std::string(62, '0') + "+1").has_value());
}

TEST_CASE("chiaki's errors map to the failures the interface can explain")
{
    CHECK(psnFailureKindOf(CHIAKI_ERR_CANCELED) == PsnFailureKind::Canceled);
    CHECK(psnFailureKindOf(CHIAKI_ERR_TIMEOUT) == PsnFailureKind::Timeout);
    CHECK(psnFailureKindOf(CHIAKI_ERR_HOST_DOWN) == PsnFailureKind::ConsoleOffline);
    CHECK(psnFailureKindOf(CHIAKI_ERR_NETWORK) == PsnFailureKind::Network);
    CHECK(psnFailureKindOf(CHIAKI_ERR_HTTP_NONOK) == PsnFailureKind::Rejected);
    CHECK(psnFailureKindOf(CHIAKI_ERR_INVALID_DATA) == PsnFailureKind::InvalidDevice);
    CHECK(psnFailureKindOf(CHIAKI_ERR_MEMORY) == PsnFailureKind::Unknown);
}

TEST_CASE("Every stage has a name for the log")
{
    CHECK(stageName(ConnectionStage::SigningIn) == "signing in");
    CHECK(stageName(ConnectionStage::PunchingData) == "opening the data channel");
}
