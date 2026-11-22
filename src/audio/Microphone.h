#pragma once

#include "audio/Devices.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <span>
#include <stop_token>
#include <string>
#include <thread>

#include <SDL3/SDL_audio.h>

namespace lynx::audio
{

// The console expects 10 ms Opus frames: 480 stereo frames at 48 kHz.
inline constexpr std::size_t kMicrophoneFramesPerBlock = 480;
inline constexpr std::size_t kMicrophoneChannels = 2;
inline constexpr std::size_t kMicrophoneBlockBytes =
    kMicrophoneFramesPerBlock * kMicrophoneChannels * sizeof(std::int16_t);

// Captured microphone audio, converted to 48 kHz stereo and handed out in
// 480-frame blocks on a thread of its own. Muting pauses the capture.
class Microphone : public AudioDeviceListener
{
public:
    using FrameCallback = std::function<void(std::span<const std::int16_t>)>;

    explicit Microphone(std::string deviceName, FrameCallback callback);
    ~Microphone() override;

    Microphone(const Microphone&) = delete;
    Microphone& operator=(const Microphone&) = delete;

    // False when Windows exposes no capture device (privacy settings can hide
    // them) or the stream cannot open.
    bool open();
    void close();
    void setMuted(bool muted);
    [[nodiscard]] bool lost() const;

    void setOnLost(std::function<void()> onLost);

    void onAudioDeviceRemoved(bool recording, std::uint32_t device) override;

private:
    static void SDLCALL captureCallback(void* user, SDL_AudioStream* stream,
                                        int additionalAmount, int totalAmount);
    void enqueue(std::span<const std::uint8_t> data);
    void drain(std::stop_token stop);

    std::string deviceName_;
    FrameCallback callback_;
    std::function<void()> onLost_;

    mutable std::mutex mutex_;
    std::condition_variable_any condition_;
    std::deque<std::uint8_t> buffer_;
    SDL_AudioStream* stream_ = nullptr;
    SDL_AudioDeviceID device_ = 0;
    bool muted_ = true;
    bool lost_ = false;
    bool overflowLogged_ = false;

    std::jthread drainThread_;
};

} // namespace lynx::audio
