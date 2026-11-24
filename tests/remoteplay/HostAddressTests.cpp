#include "remoteplay/HostAddress.h"

#include <catch2/catch_test_macros.hpp>

using mfplay::remoteplay::HostAddressError;
using mfplay::remoteplay::normalizeHostAddress;

TEST_CASE("normalizeHostAddress accepts IPv4 addresses and host names")
{
    CHECK(normalizeHostAddress("192.168.0.25") == "192.168.0.25");
    CHECK(normalizeHostAddress(" 192.168.0.25 ") == "192.168.0.25");
    CHECK(normalizeHostAddress("ps5.local") == "ps5.local");
    CHECK(normalizeHostAddress("PS5-Sala.Local") == "ps5-sala.local");
}

TEST_CASE("normalizeHostAddress tells an IPv4 part above 255 apart")
{
    CHECK(normalizeHostAddress("192.168.0.300") ==
          std::unexpected(HostAddressError::Ipv4PartOutOfRange));
}

TEST_CASE("normalizeHostAddress rejects what is neither an address nor a name")
{
    CHECK(normalizeHostAddress("") == std::unexpected(HostAddressError::Empty));
    CHECK(normalizeHostAddress("192.168.0.") == std::unexpected(HostAddressError::Invalid));
    CHECK(normalizeHostAddress("192.168.0") == std::unexpected(HostAddressError::Invalid));
    CHECK(normalizeHostAddress("1921.168.0.1") == std::unexpected(HostAddressError::Invalid));
    CHECK(normalizeHostAddress("ps5_sala") == std::unexpected(HostAddressError::Invalid));
    CHECK(normalizeHostAddress("-ps5.local") == std::unexpected(HostAddressError::Invalid));
    CHECK(normalizeHostAddress("fe80::1") == std::unexpected(HostAddressError::Invalid));
}
