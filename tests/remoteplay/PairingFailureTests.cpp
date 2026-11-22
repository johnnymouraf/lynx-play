#include "remoteplay/ConsoleRegistration.h"

#include <array>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using lynx::remoteplay::classifyPairingFailure;
using lynx::remoteplay::hostIdFromMac;
using lynx::remoteplay::PairingFailure;

namespace
{

PairingFailure classify(std::vector<std::string> log)
{
    return classifyPairingFailure(log);
}

} // namespace

TEST_CASE("classifyPairingFailure reads why chiaki's registration failed")
{
    CHECK(classify({"Regist received HTTP code 403",
                    "Reported Application Reason: 0x80108b09 (Regist failed, probably invalid "
                    "PIN)"}) == PairingFailure::WrongPin);
    CHECK(classify({"Regist received HTTP code 403",
                    "Reported Application Reason: 0x80108b02 (Invalid PSN ID)"}) ==
          PairingFailure::InvalidAccountId);
    CHECK(classify({"Regist received HTTP code 403",
                    "Reported Application Reason: 0x80108b0a (unknown)"}) ==
          PairingFailure::Rejected);
    CHECK(classify({"The console refused the registration search, keep its Link Device (PS5) "
                    "or Add Device (PS4) screen with the code open while registering"}) ==
          PairingFailure::NotInLinkScreen);
    CHECK(classify({"Regist starting search", "Regist timed out waiting for search response",
                    "Regist search failed"}) == PairingFailure::ConsoleNotFound);
    CHECK(classify({"Regist eventually failed"}) == PairingFailure::Unknown);
}

TEST_CASE("hostIdFromMac matches the host ID discovery reports")
{
    CHECK(hostIdFromMac(std::array<std::uint8_t, 6>{0x00, 0x11, 0x22, 0xAA, 0xBB, 0xCC}) ==
          "001122AABBCC");
}
