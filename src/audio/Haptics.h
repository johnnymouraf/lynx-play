#pragma once

#include "audio/Devices.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <vector>

#include <SDL3/SDL_audio.h>

namespace lynx::audio
{

enum class HapticsRoute
{
    Off,
    // The DualSense's USB audio endpoint drives its actuators.
    Audio,
    // Classic rumble, for Bluetooth or a stereo-only endpoint.
    Rumble,
};

[[nodiscard]] constexpr HapticsRoute chooseHapticsRoute(bool controllerPresent, bool wired,
                                                        bool audioEndpoint)
{
    if (!controllerPresent)
    {
        return HapticsRoute::Off;
    }

    return wired && audioEndpoint ? HapticsRoute::Audio : HapticsRoute::Rumble;
}

// Expands 3 kHz stereo haptics into the endpoint's four channels: the first
// pair (headphones) silent, the second pair (actuators) carrying the signal.
[[nodiscard]] std::vector<std::int16_t> upmixHaptics(std::span<const std::int16_t> stereo,
                                                     float gain = 1.0f);

// The loudest channel's mean absolute amplitude, as a 0..65535 rumble level.
// Values below `minimum` are silence, so the controller doesn't buzz at rest.
[[nodiscard]] std::uint16_t hapticRumbleLevel(std::span<const std::int16_t> stereo,
                                              std::uint16_t minimum = 100);

// The console's haptics stream: fine haptics through the DualSense's USB
// audio endpoint, classic rumble when only Bluetooth is available.
class Haptics : public AudioDeviceListener
{
public:
    using RumbleCallback = std::function<void(std::uint16_t left, std::uint16_t right)>;

    Haptics();
    ~Haptics() override;

    Haptics(const Haptics&) = delete;
    Haptics& operator=(const Haptics&) = delete;

    // Called from the session when the active controller changes or is
    // plugged in or out.
    void setController(bool present, bool wired);
    // Scales the signal with the console's haptic intensity, 0..1.
    void setAmplitude(float amplitude);
    void setOnRumble(RumbleCallback callback);

    // One 10 ms packet: interleaved 16-bit stereo at 3 kHz.
    void push(std::span<const std::uint8_t> packet);

    [[nodiscard]] HapticsRoute route() const;

    void onAudioDeviceAdded(bool recording) override;
    void onAudioDeviceRemoved(bool recording, std::uint32_t device) override;

private:
    void tryOpenEndpointLocked();
    void closeEndpointLocked();
    void scheduleRetryLocked();

    mutable std::mutex mutex_;
    bool controllerPresent_ = false;
    bool wired_ = false;
    bool stereoEndpointSeen_ = false;
    float amplitude_ = 1.0f;
    SDL_AudioStream* stream_ = nullptr;
    SDL_AudioDeviceID device_ = 0;
    int retryAttempt_ = 0;
    std::chrono::steady_clock::time_point nextRetry_{};
    std::chrono::steady_clock::time_point lastRumble_{};
    bool rumbling_ = false;
    bool dropLogged_ = false;
    RumbleCallback onRumble_;
};

} // namespace lynx::audio
