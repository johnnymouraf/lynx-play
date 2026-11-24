#include "audio/AudioOutput.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

namespace mfplay::audio
{

namespace
{

constexpr auto kOpenRetryDelay = std::chrono::milliseconds(500);

SDL_AudioDeviceID findDeviceByName(std::string_view name)
{
    int count = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&count);

    if (ids == nullptr)
    {
        return SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    }

    SDL_AudioDeviceID found = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;

    for (int i = 0; i < count; ++i)
    {
        const char* candidate = SDL_GetAudioDeviceName(ids[i]);

        if (candidate != nullptr && name == candidate)
        {
            found = ids[i];
            break;
        }
    }

    SDL_free(ids);
    return found;
}

} // namespace

AudioOutput::AudioOutput(std::string deviceName, int volumePercent)
    : deviceName_(std::move(deviceName)),
      volumePercent_(std::clamp(volumePercent, 0, 100))
{
    addAudioDeviceListener(this);
}

AudioOutput::~AudioOutput()
{
    removeAudioDeviceListener(this);

    const std::scoped_lock lock(mutex_);
    destroyLocked();
}

bool AudioOutput::open(std::uint32_t channels, std::uint32_t rate)
{
    if (channels == 0 || rate == 0)
    {
        return false;
    }

    const std::scoped_lock lock(mutex_);

    if (stream_ != nullptr && channels == channels_ && rate == rate_)
    {
        return true;
    }

    destroyLocked();
    channels_ = channels;
    rate_ = rate;

    if (!openLocked())
    {
        nextOpenAttempt_ = std::chrono::steady_clock::now() + kOpenRetryDelay;
        return false;
    }

    return true;
}

void AudioOutput::push(const std::int16_t* samples, std::size_t frames, std::uint32_t channels)
{
    if (samples == nullptr || frames == 0 || channels == 0)
    {
        return;
    }

    const std::scoped_lock lock(mutex_);

    if (channels != channels_)
    {
        spdlog::info("Audio format changed to {} channels; reopening the output", channels);
        destroyLocked();
        channels_ = channels;
    }

    const auto now = std::chrono::steady_clock::now();

    if (stream_ == nullptr)
    {
        if (now < nextOpenAttempt_)
        {
            return;
        }

        if (!openLocked())
        {
            nextOpenAttempt_ = now + kOpenRetryDelay;
            return;
        }
    }

    const std::size_t bytes = frames * channels * sizeof(std::int16_t);

    if (shouldDropAudio(SDL_GetAudioStreamQueued(stream_), audioQueueCap(rate_, channels_)))
    {
        SDL_ClearAudioStream(stream_);

        if (!dropLogged_)
        {
            spdlog::warn("Audio fell behind by more than {} ms; dropping the oldest audio",
                         kAudioQueueMilliseconds);
            dropLogged_ = true;
        }
    }

    if (!SDL_PutAudioStreamData(stream_, samples, static_cast<int>(bytes)) && !writeErrorLogged_)
    {
        spdlog::warn("Cannot queue game audio: {}", SDL_GetError());
        writeErrorLogged_ = true;
    }

    if (resumePending_)
    {
        const std::size_t prefill = audioQueueCap(rate_, channels_) / 3;
        const int queued = SDL_GetAudioStreamQueued(stream_);

        if (queued > 0 && static_cast<std::size_t>(queued) >= prefill)
        {
            SDL_ResumeAudioStreamDevice(stream_);
            resumePending_ = false;
        }
    }
}

void AudioOutput::setVolumePercent(int percent)
{
    const std::scoped_lock lock(mutex_);
    volumePercent_ = std::clamp(percent, 0, 100);

    if (stream_ != nullptr)
    {
        SDL_SetAudioStreamGain(stream_, static_cast<float>(volumePercent_) / 100.0f);
    }
}

void AudioOutput::setOnLost(std::function<void()> onLost)
{
    const std::scoped_lock lock(mutex_);
    onLost_ = std::move(onLost);
}

void AudioOutput::onAudioDeviceRemoved(bool recording, std::uint32_t device)
{
    std::function<void()> lost;

    {
        const std::scoped_lock lock(mutex_);

        if (recording || stream_ == nullptr || deviceName_.empty() || device != deviceId_)
        {
            return;
        }

        spdlog::warn("Audio output '{}' was removed; using the Windows default", deviceName_);
        destroyLocked();
        lost = onLost_;
    }

    if (lost)
    {
        lost();
    }
}

bool AudioOutput::openLocked()
{
    SDL_AudioDeviceID device = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    const char* openedName = "Windows default";

    if (!deviceName_.empty())
    {
        device = findDeviceByName(deviceName_);
        openedName = deviceName_.c_str();

        if (device == SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK)
        {
            spdlog::warn("Audio output '{}' is gone; using the Windows default", deviceName_);
        }
    }

    const SDL_AudioSpec format{
        .format = SDL_AUDIO_S16,
        .channels = static_cast<int>(channels_),
        .freq = static_cast<int>(rate_),
    };

    stream_ = SDL_OpenAudioDeviceStream(device, &format, nullptr, nullptr);

    if (stream_ == nullptr)
    {
        spdlog::error("Cannot open the audio output '{}': {}", openedName, SDL_GetError());
        return false;
    }

    deviceId_ = SDL_GetAudioStreamDevice(stream_);
    SDL_SetAudioStreamGain(stream_, static_cast<float>(volumePercent_) / 100.0f);
    // The device starts paused; the first frames fill a small prefill so it
    // does not underrun at once.
    resumePending_ = true;
    dropLogged_ = false;
    writeErrorLogged_ = false;

    SDL_AudioSpec deviceFormat{};
    int sampleFrames = 0;

    if (SDL_GetAudioDeviceFormat(deviceId_, &deviceFormat, &sampleFrames))
    {
        spdlog::info("Audio output '{}' open: {} Hz, {} channels requested; endpoint {} Hz, {} "
                     "channels",
                     openedName, rate_, channels_, deviceFormat.freq, deviceFormat.channels);
    }
    else
    {
        spdlog::info("Audio output '{}' open: {} Hz, {} channels requested", openedName, rate_,
                     channels_);
    }

    return true;
}

void AudioOutput::destroyLocked()
{
    if (stream_ == nullptr)
    {
        return;
    }

    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
    deviceId_ = 0;
}

} // namespace mfplay::audio
