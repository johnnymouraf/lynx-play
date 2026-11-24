#include "audio/Haptics.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

namespace mfplay::audio
{

namespace
{

constexpr std::uint32_t kHapticsRate = 3000;
constexpr std::uint32_t kHapticsChannels = 4;
constexpr std::uint32_t kHapticsMillisecondsQueued = 60;
constexpr std::size_t kHapticsQueueCap =
    kHapticsRate * kHapticsChannels * sizeof(std::int16_t) * kHapticsMillisecondsQueued / 1000;
constexpr auto kRumbleInterval = std::chrono::milliseconds(30);

constexpr std::array<int, 7> kRetryDelaysMs{500, 1000, 1000, 2000, 2000, 4000, 4000};

bool containsInsensitive(std::string_view text, std::string_view needle)
{
    const auto lower = [](char value)
    {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    };

    return std::ranges::search(text, needle, std::equal_to<>{}, lower, lower).begin() != text.end();
}

bool isDualSenseAudioDevice(std::string_view name)
{
    return containsInsensitive(name, "wireless controller") ||
           containsInsensitive(name, "dualsense");
}

std::int16_t scaled(std::int16_t sample, float gain)
{
    const float value = static_cast<float>(sample) * gain;
    return static_cast<std::int16_t>(std::clamp(value, -32768.0f, 32767.0f));
}

} // namespace

std::vector<std::int16_t> upmixHaptics(std::span<const std::int16_t> stereo, float gain)
{
    const std::size_t frames = stereo.size() / 2;
    std::vector<std::int16_t> quad(frames * kHapticsChannels);

    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        quad[frame * 4] = 0;
        quad[frame * 4 + 1] = 0;
        quad[frame * 4 + 2] = scaled(stereo[frame * 2], gain);
        quad[frame * 4 + 3] = scaled(stereo[frame * 2 + 1], gain);
    }

    return quad;
}

std::uint16_t hapticRumbleLevel(std::span<const std::int16_t> stereo, std::uint16_t minimum)
{
    const std::size_t frames = stereo.size() / 2;

    if (frames == 0)
    {
        return 0;
    }

    std::uint64_t sumLeft = 0;
    std::uint64_t sumRight = 0;

    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        sumLeft += static_cast<std::uint64_t>(std::abs(static_cast<std::int32_t>(stereo[frame * 2])));
        sumRight +=
            static_cast<std::uint64_t>(std::abs(static_cast<std::int32_t>(stereo[frame * 2 + 1])));
    }

    const std::uint64_t meanLeft = sumLeft / frames;
    const std::uint64_t meanRight = sumRight / frames;
    const std::uint64_t loudest = std::max(meanLeft, meanRight);

    if (loudest < minimum)
    {
        return 0;
    }

    return static_cast<std::uint16_t>(std::min<std::uint64_t>(loudest * 2, 65535));
}

Haptics::Haptics()
{
    addAudioDeviceListener(this);
}

Haptics::~Haptics()
{
    removeAudioDeviceListener(this);

    const std::scoped_lock lock(mutex_);
    closeEndpointLocked();
}

void Haptics::setController(bool present, bool wired)
{
    const std::scoped_lock lock(mutex_);

    if (present == controllerPresent_ && wired == wired_)
    {
        return;
    }

    const bool wasPresent = controllerPresent_;
    controllerPresent_ = present;
    wired_ = wired;

    if (!present || !wired)
    {
        closeEndpointLocked();
        retryAttempt_ = 0;
        retryExhausted_ = false;
    }
    else if (stream_ == nullptr)
    {
        retryAttempt_ = 0;
        retryExhausted_ = false;
        nextRetry_ = std::chrono::steady_clock::now();
        tryOpenEndpointLocked();
    }

    if (present && !wasPresent)
    {
        spdlog::info("DualSense feedback: {} haptics", wired ? "USB" : "Bluetooth");
    }

    if (wasPresent && !present && onRumble_ && rumbling_)
    {
        rumbling_ = false;
        onRumble_(0, 0);
    }
}

void Haptics::setAmplitude(float amplitude)
{
    const std::scoped_lock lock(mutex_);
    amplitude_ = std::clamp(amplitude, 0.0f, 1.0f);
}

void Haptics::setOnRumble(RumbleCallback callback)
{
    const std::scoped_lock lock(mutex_);
    onRumble_ = std::move(callback);
}

void Haptics::push(std::span<const std::uint8_t> packet)
{
    const std::scoped_lock lock(mutex_);

    if (!controllerPresent_ || packet.empty())
    {
        return;
    }

    if ((packet.size() % (2 * sizeof(std::int16_t))) != 0)
    {
        spdlog::warn("Haptics packet has an unexpected size: {}", packet.size());
        return;
    }

    if (wired_ && stream_ == nullptr && !retryExhausted_ &&
        std::chrono::steady_clock::now() >= nextRetry_)
    {
        tryOpenEndpointLocked();
    }

    std::vector<std::int16_t> stereo(packet.size() / sizeof(std::int16_t));
    std::memcpy(stereo.data(), packet.data(), packet.size());

    if (stream_ != nullptr)
    {
        if (amplitude_ <= 0.0f)
        {
            return;
        }

        const std::vector<std::int16_t> quad = upmixHaptics(stereo, amplitude_);

        if (SDL_GetAudioStreamQueued(stream_) > static_cast<int>(kHapticsQueueCap))
        {
            SDL_ClearAudioStream(stream_);

            if (!dropLogged_)
            {
                spdlog::warn("Haptics fell behind; dropping the oldest haptics");
                dropLogged_ = true;
            }
        }

        SDL_PutAudioStreamData(stream_, quad.data(),
                                static_cast<int>(quad.size() * sizeof(std::int16_t)));
        return;
    }

    if (!onRumble_ || amplitude_ <= 0.0f)
    {
        return;
    }

    std::uint16_t level = hapticRumbleLevel(stereo);
    level = static_cast<std::uint16_t>(std::min<std::uint32_t>(
        static_cast<std::uint32_t>(level) * static_cast<std::uint32_t>(amplitude_ * 100.0f) / 100,
        65535));

    const auto now = std::chrono::steady_clock::now();

    if (now - lastRumble_ < kRumbleInterval)
    {
        return;
    }

    if (level > 0)
    {
        lastRumble_ = now;
        rumbling_ = true;
        onRumble_(level, level);
        return;
    }

    if (rumbling_)
    {
        lastRumble_ = now;
        rumbling_ = false;
        onRumble_(0, 0);
    }
}

HapticsRoute Haptics::route() const
{
    const std::scoped_lock lock(mutex_);
    return chooseHapticsRoute(controllerPresent_, wired_, stream_ != nullptr);
}

void Haptics::onAudioDeviceAdded(bool recording)
{
    if (recording)
    {
        return;
    }

    const std::scoped_lock lock(mutex_);

    if (!wired_ || stream_ != nullptr)
    {
        return;
    }

    retryAttempt_ = 0;
    retryExhausted_ = false;
    nextRetry_ = std::chrono::steady_clock::now();
    tryOpenEndpointLocked();
}

void Haptics::onAudioDeviceRemoved(bool recording, std::uint32_t device)
{
    if (recording)
    {
        return;
    }

    const std::scoped_lock lock(mutex_);

    if (stream_ == nullptr || device != device_)
    {
        return;
    }

    spdlog::warn("The DualSense audio endpoint went away; haptics use rumble");
    closeEndpointLocked();
    retryAttempt_ = 0;
    retryExhausted_ = false;
    scheduleRetryLocked();
}

void Haptics::tryOpenEndpointLocked()
{
    int count = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&count);

    if (ids == nullptr)
    {
        scheduleRetryLocked();
        return;
    }

    SDL_AudioDeviceID chosen = 0;
    std::string chosenName;

    for (int i = 0; i < count; ++i)
    {
        const char* name = SDL_GetAudioDeviceName(ids[i]);

        if (name == nullptr || !isDualSenseAudioDevice(name))
        {
            continue;
        }

        SDL_AudioSpec format{};
        int sampleFrames = 0;

        if (!SDL_GetAudioDeviceFormat(ids[i], &format, &sampleFrames))
        {
            continue;
        }

        spdlog::info("DualSense audio endpoint '{}': {} Hz, {} channels", name, format.freq,
                     format.channels);

        if (format.channels == static_cast<int>(kHapticsChannels))
        {
            chosen = ids[i];
            chosenName = name;
            break;
        }

        if (!stereoEndpointSeen_)
        {
            spdlog::warn("The DualSense audio endpoint is not 4-channel; haptics use rumble");
            stereoEndpointSeen_ = true;
        }
    }

    SDL_free(ids);

    if (chosen == 0)
    {
        scheduleRetryLocked();
        return;
    }

    const SDL_AudioSpec format{
        .format = SDL_AUDIO_S16,
        .channels = static_cast<int>(kHapticsChannels),
        .freq = static_cast<int>(kHapticsRate),
    };

    stream_ = SDL_OpenAudioDeviceStream(chosen, &format, nullptr, nullptr);

    if (stream_ == nullptr)
    {
        spdlog::warn("Cannot open the DualSense audio endpoint '{}': {}", chosenName,
                     SDL_GetError());
        scheduleRetryLocked();
        return;
    }

    device_ = SDL_GetAudioStreamDevice(stream_);
    SDL_ResumeAudioStreamDevice(stream_);
    retryAttempt_ = 0;
    retryExhausted_ = false;
    dropLogged_ = false;
    spdlog::info("Haptics audio on '{}' (3 kHz stereo to 4-channel 48 kHz)", chosenName);
}

void Haptics::closeEndpointLocked()
{
    if (stream_ == nullptr)
    {
        return;
    }

    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
    device_ = 0;
}

void Haptics::scheduleRetryLocked()
{
    if (retryAttempt_ >= static_cast<int>(kRetryDelaysMs.size()))
    {
        if (!retryExhausted_)
        {
            spdlog::warn("The DualSense audio endpoint did not appear; haptics use rumble");
            retryExhausted_ = true;
        }

        return;
    }

    nextRetry_ = std::chrono::steady_clock::now() +
                 std::chrono::milliseconds(kRetryDelaysMs[static_cast<std::size_t>(retryAttempt_)]);
    ++retryAttempt_;
}

} // namespace mfplay::audio
