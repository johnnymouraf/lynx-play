#include "remoteplay/PsnDevices.h"

#include "remoteplay/ConsoleMerge.h"

#include <catch2/catch_test_macros.hpp>

using namespace mfplay::remoteplay;

namespace
{

SavedConsole console(std::string hostId, std::string name, bool paired)
{
    SavedConsole saved;
    saved.hostId = std::move(hostId);
    saved.hostName = std::move(name);

    if (paired)
    {
        saved.credentials = Credentials{};
    }

    return saved;
}

} // namespace

TEST_CASE("A paired console takes the DUID of the PSN device with its name")
{
    const std::vector<PsnDevice> devices{{.name = "PS5-001", .duid = "aa"}};
    const std::vector<SavedConsole> consoles{console("0011", "PS5-001", true)};

    const auto matches = matchPsnDevices(devices, consoles);

    REQUIRE(matches.size() == 1);
    CHECK(matches.front() == PsnMatch{.consoleKey = consoleKey(consoles.front()), .duid = "aa"});
}

TEST_CASE("A console no PSN device is named after matches nothing")
{
    const std::vector<PsnDevice> devices{{.name = "PS5-002", .duid = "bb"}};
    const std::vector<SavedConsole> consoles{console("0011", "PS5-001", true)};

    CHECK(matchPsnDevices(devices, consoles).empty());
}

TEST_CASE("A console that isn't paired is ignored")
{
    const std::vector<PsnDevice> devices{{.name = "PS5-001", .duid = "aa"}};
    const std::vector<SavedConsole> consoles{console("0011", "PS5-001", false)};

    CHECK(matchPsnDevices(devices, consoles).empty());
}

TEST_CASE("A name shared by several devices or paired consoles is ambiguous")
{
    const std::vector<PsnDevice> twinDevices{{.name = "PS5", .duid = "aa"},
                                             {.name = "PS5", .duid = "bb"}};
    const std::vector<PsnDevice> oneDevice{{.name = "PS5", .duid = "aa"}};
    const std::vector<SavedConsole> oneConsole{console("0011", "PS5", true)};
    const std::vector<SavedConsole> twinConsoles{console("0011", "PS5", true),
                                                 console("0022", "PS5", true)};

    CHECK(matchPsnDevices(twinDevices, oneConsole).empty());
    CHECK(matchPsnDevices(oneDevice, twinConsoles).empty());
}
