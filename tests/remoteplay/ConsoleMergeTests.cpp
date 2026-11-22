#include "remoteplay/ConsoleMerge.h"

#include <catch2/catch_test_macros.hpp>

using namespace lynx::remoteplay;

namespace
{

const Clock::time_point kNow{std::chrono::seconds(1'790'000'000)};

SavedConsole pairedConsole()
{
    SavedConsole console;
    console.hostId = "001122334455";
    console.hostName = "PS5-123";
    console.address = "192.168.0.25";
    console.credentials = Credentials{};
    return console;
}

DiscoveredHost answeringHost(std::string hostId, std::string address)
{
    DiscoveredHost host;
    host.hostId = std::move(hostId);
    host.name = "PS5-123";
    host.address = std::move(address);
    host.state = HostState::Standby;
    host.ps5 = true;
    return host;
}

} // namespace

TEST_CASE("A saved console that answers from a new address takes that address")
{
    const auto result = mergeConsoles({pairedConsole()},
                                      {answeringHost("001122334455", "192.168.0.40")}, kNow);

    REQUIRE(result.entries.size() == 1);
    CHECK(result.entries[0].savedIndex == 0U);
    CHECK(result.entries[0].discovered.has_value());
    CHECK(result.saved[0].address == "192.168.0.40");
    CHECK(result.saved[0].lastSeen == kNow);
    CHECK(result.savedChanged);
}

TEST_CASE("A saved console that answers from the same address leaves nothing to save")
{
    const auto result = mergeConsoles({pairedConsole()},
                                      {answeringHost("001122334455", "192.168.0.25")}, kNow);

    CHECK_FALSE(result.savedChanged);
}

TEST_CASE("A saved console that doesn't answer is listed without a host")
{
    const auto result = mergeConsoles({pairedConsole()}, {}, kNow);

    REQUIRE(result.entries.size() == 1);
    CHECK(result.entries[0].key == "001122334455");
    CHECK_FALSE(result.entries[0].discovered.has_value());
    CHECK_FALSE(result.saved[0].lastSeen.has_value());
}

TEST_CASE("A console that answers but isn't saved is listed after the saved ones")
{
    const auto result = mergeConsoles({pairedConsole()},
                                      {answeringHost("AABBCCDDEEFF", "192.168.0.50"),
                                       answeringHost("001122334455", "192.168.0.25")},
                                      kNow);

    REQUIRE(result.entries.size() == 2);
    CHECK(result.entries[0].key == "001122334455");
    CHECK(result.entries[1].key == "AABBCCDDEEFF");
    CHECK_FALSE(result.entries[1].savedIndex.has_value());
    CHECK(result.saved.size() == 1);
}

TEST_CASE("An address added by hand takes the host ID of the console that answers there")
{
    SavedConsole manual;
    manual.manualHost = "ps5.local";
    DiscoveredHost host = answeringHost("AABBCCDDEEFF", "192.168.0.50");
    host.queriedHost = "ps5.local";

    CHECK(consoleKey(manual) == "manual:ps5.local");

    const auto result = mergeConsoles({manual}, {host}, kNow);

    REQUIRE(result.saved.size() == 1);
    CHECK(result.saved[0].hostId == "AABBCCDDEEFF");
    CHECK(result.saved[0].address == "192.168.0.50");
    CHECK(result.entries[0].key == "AABBCCDDEEFF");
    CHECK(result.savedChanged);
}

TEST_CASE("An address added by hand for a console already saved joins that console")
{
    SavedConsole manual;
    manual.manualHost = "192.168.0.25";

    const auto result = mergeConsoles({pairedConsole(), manual},
                                      {answeringHost("001122334455", "192.168.0.25")}, kNow);

    REQUIRE(result.saved.size() == 1);
    CHECK(result.saved[0].manualHost == "192.168.0.25");
    CHECK(result.saved[0].credentials.has_value());
    CHECK(result.entries.size() == 1);
    CHECK(result.savedChanged);
}
