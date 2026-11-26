#include "remoteplay/PsnPairing.h"

#include <algorithm>

namespace mfplay::remoteplay
{

std::vector<PsnDevice> unpairedPsnDevices(std::span<const PsnDevice> devices,
                                          std::span<const SavedConsole> consoles,
                                          std::span<const DiscoveredHost> discovered)
{
    const std::vector<PsnMatch> matches = matchPsnDevices(devices, consoles);
    std::vector<PsnDevice> unpaired;

    for (const PsnDevice& device : devices)
    {
        const bool paired = std::ranges::contains(matches, device.duid, &PsnMatch::duid);
        const bool answersHere = std::ranges::contains(discovered, device.name,
                                                       &DiscoveredHost::name);

        if (!paired && !answersHere)
        {
            unpaired.push_back(device);
        }
    }

    return unpaired;
}

SavedConsole withPsnRegistration(SavedConsole console, const PairedConsole& paired,
                                 std::string_view deviceName)
{
    console.hostId = hostIdFromMac(paired.serverMac);
    console.hostName = std::string(deviceName);
    console.ps5 = true;
    console.serverMac = paired.serverMac;
    console.credentials = paired.credentials;

    if (paired.target != 0)
    {
        console.target = paired.target;
    }

    return console;
}

} // namespace mfplay::remoteplay
