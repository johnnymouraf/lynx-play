#pragma once

#include "audio/Devices.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

#include <SDL3/SDL_audio.h>

namespace lynx::audio
{

// How much decoded audio may wait in SDL's queue before older audio is
// dropped. The stream runs at the console's 48 kHz; two or three 10 ms
// frames are the target, and this is the hard limit.
inline constexpr std::uint32_t kAudioQueueMilliseconds = 60;

[[nodiscard]] constexpr std::size_t audioQueueCap(std::uint32_t rate, std::uint32_t channels)
{
    return static_cast<std::size_t>(rate) * channels * sizeof(std::int16_t) *
           kAudioQueueMilliseconds / 1000;
}

[[nodiscard]] constexpr bool shouldDropAudio(int queuedBytes, std::size_t cap)
{
    return queuedBytes > 0 && static_cast<std::size_t>(queuedBytes) > cap;
}

// The game's sound through an SDL audio stream, at the format chiaki decodes
// (48 kHz stereo for the PS5). Pushes come from chiaki's audio thread; SDL's
// stream does the conversion to the chosen Windows endpoint.
class AudioOutput : public AudioDeviceListener
{
public:
    AudioOutput(std::string deviceName, int volumePercent);
    ~AudioOutput() override;

    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    // Called when the stream header arrives. False when no endpoint opened.
    bool open(std::uint32_t channels, std::uint32_t rate);
    void push(const std::int16_t* samples, std::size_t frames, std::uint32_t channels);
    void setVolumePercent(int percent);

    // Reports a chosen endpoint disappearing; the next frame reopens the
    // system default.
    void setOnLost(std::function<void()> onLost);

    void onAudioDeviceRemoved(bool recording, std::uint32_t device) override;

private:
    bool openLocked();
    void destroyLocked();
    [[nodiscard]] SDL_AudioDeviceID resolveDeviceLocked() const;

    mutable std::mutex mutex_;
    std::string deviceName_;
    int volumePercent_ = 100;
    std::uint32_t channels_ = 2;
    std::uint32_t rate_ = 48000;
    SDL_AudioStream* stream_ = nullptr;
    SDL_AudioDeviceID deviceId_ = 0;
    std::chrono::steady_clock::time_point nextOpenAttempt_{};
    bool dropLogged_ = false;
    bool writeErrorLogged_ = false;
    std::function<void()> onLost_;
};

} // namespace lynx::audio
