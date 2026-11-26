#pragma once

#include "remoteplay/Console.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mfplay::remoteplay
{

// One console of the home screen: saved, answering, or both.
struct ConsoleEntry
{
    std::string key;
    // Index into MergeResult::saved.
    std::optional<std::size_t> savedIndex;
    std::optional<DiscoveredHost> discovered;

    bool operator==(const ConsoleEntry&) const = default;
};

struct MergeResult
{
    std::vector<SavedConsole> saved;
    // The saved consoles in their order, then the unsaved ones that answer,
    // by name.
    std::vector<ConsoleEntry> entries;
    // A saved console answered from a new address or for the first time, or
    // two saved entries turned out to be the same console: save the file.
    bool savedChanged = false;
};

// Whether the host answered a direct query of the console's remote address,
// rather than the local network. Such an answer carries an address that is
// not on the local network, so it never replaces the saved one.
[[nodiscard]] bool isRemoteAnswer(const DiscoveredHost& host, std::string_view remoteHost);

// Matches the consoles that answer with the saved ones: by host ID, or, for
// an address typed by hand that never answered before, by that address. The
// saved consoles that answer get their address, name and version refreshed
// and `now` as their last-seen time. A remote answer refreshes everything but
// the address.
[[nodiscard]] MergeResult mergeConsoles(std::vector<SavedConsole> saved,
                                        const std::vector<DiscoveredHost>& discovered,
                                        Clock::time_point now);

// The host ID, or "manual:<address>" for an address that hasn't answered yet.
[[nodiscard]] std::string consoleKey(const SavedConsole& console);

} // namespace mfplay::remoteplay
