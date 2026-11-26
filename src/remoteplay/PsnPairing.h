#pragma once

#include "remoteplay/Console.h"
#include "remoteplay/ConsoleRegistration.h"
#include "remoteplay/PsnDevices.h"

#include <span>
#include <string_view>
#include <vector>

namespace mfplay::remoteplay
{

// The PS5s of the account that no paired console here is, and that no console
// answering on the local network has the name of. These are the ones to pair
// over the internet; the others already have a card.
[[nodiscard]] std::vector<PsnDevice> unpairedPsnDevices(
    std::span<const PsnDevice> devices, std::span<const SavedConsole> consoles,
    std::span<const DiscoveredHost> discovered);

// The console after a registration over the PSN: its MAC is its host ID, the
// PSN device's name is its host name, and the registration's keys make it
// paired. A target of 0 keeps the one it had; the nickname, address and times
// stay as they were.
[[nodiscard]] SavedConsole withPsnRegistration(SavedConsole console,
                                               const PairedConsole& paired,
                                               std::string_view deviceName);

} // namespace mfplay::remoteplay
