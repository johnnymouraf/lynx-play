#include "remoteplay/PsnDevices.h"

#include "remoteplay/ChiakiLogSink.h"
#include "remoteplay/ConsoleMerge.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <map>

#include <chiaki/remote/holepunch.h>

namespace mfplay::remoteplay
{

namespace
{

constexpr int kListAttempts = 3;

std::string toHex(const uint8_t* bytes, std::size_t size)
{
    std::string hex;
    hex.reserve(size * 2);

    for (std::size_t index = 0; index < size; ++index)
    {
        hex += std::format("{:02x}", bytes[index]);
    }

    return hex;
}

PsnDevice deviceFrom(const ChiakiHolepunchDeviceInfo& info)
{
    return PsnDevice{
        .name = std::string(info.device_name, strnlen(info.device_name, sizeof(info.device_name))),
        .duid = toHex(info.device_uid, sizeof(info.device_uid)),
    };
}

} // namespace

std::expected<std::vector<PsnDevice>, std::string> listPsnDevices(std::string_view accessToken)
{
    const std::string token(accessToken);

    ChiakiLog log;
    chiaki_log_init(&log, 0, discardChiakiLog, nullptr);

    ChiakiHolepunchDeviceInfo* infos = nullptr;
    std::size_t count = 0;
    ChiakiErrorCode result = CHIAKI_ERR_UNKNOWN;

    for (int attempt = 0; attempt < kListAttempts && result != CHIAKI_ERR_SUCCESS; ++attempt)
    {
        result = chiaki_holepunch_list_devices(token.c_str(), CHIAKI_HOLEPUNCH_CONSOLE_TYPE_PS5,
                                               &infos, &count, &log);
    }

    if (result != CHIAKI_ERR_SUCCESS)
    {
        return std::unexpected(std::format("chiaki_holepunch_list_devices failed {} times: {}",
                                           kListAttempts, chiaki_error_string(result)));
    }

    std::vector<PsnDevice> devices;

    for (std::size_t index = 0; index < count; ++index)
    {
        if (infos[index].remoteplay_enabled)
        {
            devices.push_back(deviceFrom(infos[index]));
        }
    }

    if (infos != nullptr)
    {
        chiaki_holepunch_free_device_list(&infos);
    }

    return devices;
}

std::vector<PsnMatch> matchPsnDevices(std::span<const PsnDevice> devices,
                                      std::span<const SavedConsole> consoles)
{
    std::map<std::string, int> deviceCounts;
    std::map<std::string, int> consoleCounts;

    for (const PsnDevice& device : devices)
    {
        ++deviceCounts[device.name];
    }

    for (const SavedConsole& console : consoles)
    {
        if (console.credentials)
        {
            ++consoleCounts[console.hostName];
        }
    }

    std::vector<PsnMatch> matches;

    for (const SavedConsole& console : consoles)
    {
        if (!console.credentials || consoleCounts.at(console.hostName) != 1)
        {
            continue;
        }

        const auto device = std::ranges::find(devices, console.hostName, &PsnDevice::name);

        if (device == devices.end() || deviceCounts.at(device->name) != 1)
        {
            continue;
        }

        matches.push_back({.consoleKey = consoleKey(console), .duid = device->duid});
    }

    return matches;
}

} // namespace mfplay::remoteplay
