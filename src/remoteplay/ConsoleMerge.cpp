#include "remoteplay/ConsoleMerge.h"

#include <algorithm>
#include <utility>

namespace lynx::remoteplay
{

namespace
{

constexpr std::string_view kManualKeyPrefix = "manual:";

const DiscoveredHost* findById(const std::vector<DiscoveredHost>& discovered,
                               std::string_view hostId)
{
    const auto host = std::ranges::find(discovered, hostId, &DiscoveredHost::hostId);
    return host != discovered.end() ? &*host : nullptr;
}

// The console a direct query of `manualHost` found, or the broadcast found at
// that very address.
const DiscoveredHost* findByQuery(const std::vector<DiscoveredHost>& discovered,
                                  std::string_view manualHost)
{
    const auto host = std::ranges::find_if(discovered, [manualHost](const DiscoveredHost& item) {
        return item.queriedHost == manualHost || item.address == manualHost;
    });
    return host != discovered.end() ? &*host : nullptr;
}

// Copies what the console now reports; true when something changed.
bool refresh(SavedConsole& console, const DiscoveredHost& host)
{
    const bool changed = console.address != host.address || console.hostName != host.name ||
                         console.ps5 != host.ps5 || console.target != host.target ||
                         console.systemVersion != host.systemVersion;

    console.address = host.address;
    console.hostName = host.name;
    console.ps5 = host.ps5;
    console.target = host.target;
    console.systemVersion = host.systemVersion;

    return changed;
}

// Gives a host ID to the addresses typed by hand that just answered. One
// that turns out to be a console already saved is folded into it.
bool identifyManualConsoles(std::vector<SavedConsole>& saved,
                            const std::vector<DiscoveredHost>& discovered)
{
    std::vector<bool> folded(saved.size(), false);
    bool changed = false;

    for (std::size_t i = 0; i < saved.size(); ++i)
    {
        SavedConsole& console = saved[i];

        if (!console.hostId.empty() || console.manualHost.empty())
        {
            continue;
        }

        const DiscoveredHost* host = findByQuery(discovered, console.manualHost);

        if (host == nullptr)
        {
            continue;
        }

        changed = true;
        const auto existing = std::ranges::find(saved, host->hostId, &SavedConsole::hostId);

        if (existing == saved.end())
        {
            console.hostId = host->hostId;
            continue;
        }

        if (existing->manualHost.empty())
        {
            existing->manualHost = console.manualHost;
        }

        folded[i] = true;
    }

    std::vector<SavedConsole> kept;

    for (std::size_t i = 0; i < saved.size(); ++i)
    {
        if (!folded[i])
        {
            kept.push_back(std::move(saved[i]));
        }
    }

    saved = std::move(kept);
    return changed;
}

std::vector<ConsoleEntry> unsavedEntries(const std::vector<SavedConsole>& saved,
                                         const std::vector<DiscoveredHost>& discovered)
{
    std::vector<ConsoleEntry> entries;

    for (const DiscoveredHost& host : discovered)
    {
        const bool isSaved = std::ranges::contains(saved, host.hostId, &SavedConsole::hostId);
        const bool isListed = std::ranges::contains(entries, host.hostId, &ConsoleEntry::key);

        if (!isSaved && !isListed)
        {
            entries.push_back({.key = host.hostId, .savedIndex = std::nullopt, .discovered = host});
        }
    }

    std::ranges::sort(entries, {}, [](const ConsoleEntry& entry) -> const std::string& {
        return entry.discovered->name;
    });
    return entries;
}

} // namespace

MergeResult mergeConsoles(std::vector<SavedConsole> saved,
                          const std::vector<DiscoveredHost>& discovered, Clock::time_point now)
{
    MergeResult result;
    result.savedChanged = identifyManualConsoles(saved, discovered);

    for (SavedConsole& console : saved)
    {
        const DiscoveredHost* host = findById(discovered, console.hostId);

        if (console.hostId.empty() || host == nullptr)
        {
            continue;
        }

        console.lastSeen = now;
        result.savedChanged = refresh(console, *host) || result.savedChanged;
    }

    for (std::size_t i = 0; i < saved.size(); ++i)
    {
        const DiscoveredHost* host = findById(discovered, saved[i].hostId);
        ConsoleEntry entry{
            .key = consoleKey(saved[i]),
            .savedIndex = i,
            .discovered = std::nullopt,
        };

        if (!saved[i].hostId.empty() && host != nullptr)
        {
            entry.discovered = *host;
        }

        result.entries.push_back(std::move(entry));
    }

    result.entries.append_range(unsavedEntries(saved, discovered));
    result.saved = std::move(saved);
    return result;
}

std::string consoleKey(const SavedConsole& console)
{
    if (!console.hostId.empty())
    {
        return console.hostId;
    }

    return std::string(kManualKeyPrefix) + console.manualHost;
}

} // namespace lynx::remoteplay
