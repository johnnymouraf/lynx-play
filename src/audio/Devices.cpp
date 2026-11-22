#include "audio/Devices.h"

#include <algorithm>
#include <mutex>
#include <utility>

#include <SDL3/SDL.h>

namespace lynx::audio
{

namespace
{

std::mutex g_listenersMutex;
std::vector<AudioDeviceListener*> g_listeners;

std::vector<AudioDeviceInfo> enumerate(bool recording)
{
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0)
    {
        return {};
    }

    int count = 0;
    SDL_AudioDeviceID* ids =
        recording ? SDL_GetAudioRecordingDevices(&count) : SDL_GetAudioPlaybackDevices(&count);

    if (ids == nullptr)
    {
        return {};
    }

    std::vector<AudioDeviceInfo> devices;
    devices.reserve(static_cast<std::size_t>(count));

    for (int i = 0; i < count; ++i)
    {
        const char* name = SDL_GetAudioDeviceName(ids[i]);

        if (name == nullptr)
        {
            continue;
        }

        SDL_AudioSpec format{};
        int sampleFrames = 0;

        if (!SDL_GetAudioDeviceFormat(ids[i], &format, &sampleFrames))
        {
            continue;
        }

        devices.push_back({.name = name, .channels = format.channels, .sampleRate = format.freq});
    }

    SDL_free(ids);
    return devices;
}

} // namespace

std::vector<AudioDeviceInfo> outputDevices()
{
    return enumerate(false);
}

std::vector<AudioDeviceInfo> inputDevices()
{
    return enumerate(true);
}

void addAudioDeviceListener(AudioDeviceListener* listener)
{
    const std::scoped_lock lock(g_listenersMutex);
    g_listeners.push_back(listener);
}

void removeAudioDeviceListener(AudioDeviceListener* listener)
{
    const std::scoped_lock lock(g_listenersMutex);
    std::erase(g_listeners, listener);
}

void dispatchAudioDeviceEvent(bool added, bool recording, std::uint32_t device)
{
    std::vector<AudioDeviceListener*> listeners;

    {
        const std::scoped_lock lock(g_listenersMutex);
        listeners = g_listeners;
    }

    for (AudioDeviceListener* listener : listeners)
    {
        if (added)
        {
            listener->onAudioDeviceAdded(recording);
            continue;
        }

        listener->onAudioDeviceRemoved(recording, device);
    }
}

} // namespace lynx::audio
