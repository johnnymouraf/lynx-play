#include "remoteplay/PsnPairing.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

using namespace mfplay::remoteplay;

namespace
{

// A PS5 saved here as paired, under the name the PSN gives it.
SavedConsole pairedConsole(std::string name)
{
    SavedConsole saved;
    saved.hostId = "0011";
    saved.hostName = std::move(name);
    saved.credentials = Credentials{};
    return saved;
}

DiscoveredHost answeringHere(std::string name)
{
    DiscoveredHost host;
    host.name = std::move(name);
    return host;
}

PairedConsole registration()
{
    PairedConsole paired;
    paired.credentials.registKey[0] = 0x11;
    paired.credentials.rpKey[0] = 0x22;
    paired.credentials.rpKeyType = 1;
    paired.serverMac = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
    paired.target = 42;
    return paired;
}

} // namespace

TEST_CASE("A PS5 a paired console is matched to is not offered for pairing")
{
    const std::vector<PsnDevice> devices{{.name = "PS5-001", .duid = "aa"}};
    const std::vector<SavedConsole> consoles{pairedConsole("PS5-001")};

    CHECK(unpairedPsnDevices(devices, consoles, {}).empty());
}

TEST_CASE("A PS5 that answers on the local network under its name is not offered")
{
    const std::vector<PsnDevice> devices{{.name = "PS5-002", .duid = "bb"}};
    const std::vector<DiscoveredHost> discovered{answeringHere("PS5-002")};

    CHECK(unpairedPsnDevices(devices, {}, discovered).empty());
}

TEST_CASE("The PS5s nothing here is paired with keep the account's order")
{
    const std::vector<PsnDevice> devices{{.name = "PS5-001", .duid = "aa"},
                                         {.name = "PS5-002", .duid = "bb"},
                                         {.name = "PS5-003", .duid = "cc"},
                                         {.name = "PS5-004", .duid = "dd"}};
    const std::vector<SavedConsole> consoles{pairedConsole("PS5-001")};
    const std::vector<DiscoveredHost> discovered{answeringHere("PS5-003")};

    const std::vector<PsnDevice> unpaired = unpairedPsnDevices(devices, consoles, discovered);

    REQUIRE(unpaired.size() == 2);
    CHECK(unpaired[0] == PsnDevice{.name = "PS5-002", .duid = "bb"});
    CHECK(unpaired[1] == PsnDevice{.name = "PS5-004", .duid = "dd"});
}

TEST_CASE("A registration keys the console by its MAC and names it after the PSN device")
{
    const PairedConsole paired = registration();

    const SavedConsole console = withPsnRegistration(SavedConsole{}, paired, "PS5-002");

    CHECK(console.hostId == "A1B2C3D4E5F6");
    CHECK(console.hostName == "PS5-002");
    CHECK(console.serverMac == paired.serverMac);
    CHECK(console.credentials == paired.credentials);
    CHECK(console.target == 42);
    CHECK(console.ps5);
}

TEST_CASE("A registration keeps the nickname, address and times of the console it pairs")
{
    SavedConsole saved;
    saved.hostId = "A1B2C3D4E5F6";
    saved.nickname = "Sala";
    saved.address = "192.168.0.25";
    saved.lastSeen = Clock::time_point{std::chrono::seconds(60)};

    const SavedConsole console = withPsnRegistration(saved, registration(), "PS5-002");

    CHECK(console.nickname == "Sala");
    CHECK(console.address == "192.168.0.25");
    CHECK(console.lastSeen == saved.lastSeen);
    CHECK(console.credentials.has_value());
}
