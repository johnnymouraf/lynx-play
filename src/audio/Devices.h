#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mfplay::audio
{

struct AudioDeviceInfo
{
    std::string name;
    // The endpoint's Windows mix format, which SDL opens and converts to.
    int channels = 0;
    int sampleRate = 0;
};

// Physical playback and recording endpoints; empty when SDL audio is off.
[[nodiscard]] std::vector<AudioDeviceInfo> outputDevices();
[[nodiscard]] std::vector<AudioDeviceInfo> inputDevices();

// Hotplug notifications for the session's audio streams. Register on the
// thread that will receive them (the app's event pump).
class AudioDeviceListener
{
public:
    AudioDeviceListener() = default;
    AudioDeviceListener(const AudioDeviceListener&) = delete;
    AudioDeviceListener& operator=(const AudioDeviceListener&) = delete;
    virtual ~AudioDeviceListener() = default;

    virtual void onAudioDeviceAdded(bool recording)
    {
        static_cast<void>(recording);
    }

    virtual void onAudioDeviceRemoved(bool recording, std::uint32_t device)
    {
        static_cast<void>(recording);
        static_cast<void>(device);
    }
};

void addAudioDeviceListener(AudioDeviceListener* listener);
void removeAudioDeviceListener(AudioDeviceListener* listener);

// Called by the app's SDL event pump for each audio device event.
void dispatchAudioDeviceEvent(bool added, bool recording, std::uint32_t device);

} // namespace mfplay::audio
