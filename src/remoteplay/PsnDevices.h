#pragma once

#include "remoteplay/Console.h"

#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mfplay::remoteplay
{

// A PS5 of the signed-in PSN account that accepts Remote Play.
struct PsnDevice
{
    std::string name;
    // The device's identifier in hex.
    std::string duid;

    bool operator==(const PsnDevice&) const = default;
};

// A saved console the PSN lists, by its card key.
struct PsnMatch
{
    std::string consoleKey;
    std::string duid;

    bool operator==(const PsnMatch&) const = default;
};

// The account's PS5s, as the PSN lists them. Blocks on HTTP: call it off the
// UI thread.
[[nodiscard]] std::expected<std::vector<PsnDevice>, std::string> listPsnDevices(
    std::string_view accessToken);

// Pairs each paired console with the PSN device that has its name. A name that
// more than one device or paired console has is ambiguous and matches none.
[[nodiscard]] std::vector<PsnMatch> matchPsnDevices(std::span<const PsnDevice> devices,
                                                    std::span<const SavedConsole> consoles);

} // namespace mfplay::remoteplay
