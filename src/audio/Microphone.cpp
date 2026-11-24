#include "audio/Microphone.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <utility>

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

namespace mfplay::audio
{

namespace
{

// A slow consumer drops stale speech instead of sending it late.
constexpr std::size_t kBacklogBytes = kMicrophoneBlockBytes * 10;

SDL_AudioDeviceID findRecordingDevice(std::string_view name)
{
    int count = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioRecordingDevices(&count);

    if (ids == nullptr)
    {
        return SDL_AUDIO_DEVICE_DEFAULT_RECORDING;
    }

    SDL_AudioDeviceID found = SDL_AUDIO_DEVICE_DEFAULT_RECORDING;

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

Microphone::Microphone(std::string deviceName, FrameCallback callback)
    : deviceName_(std::move(deviceName)), callback_(std::move(callback)),
      drainThread_([this](std::stop_token stop) { drain(stop); })
{
    addAudioDeviceListener(this);
}

Microphone::~Microphone()
{
    removeAudioDeviceListener(this);

    drainThread_.request_stop();
    condition_.notify_all();
    drainThread_ = std::jthread{};

    SDL_AudioStream* stream = nullptr;

    {
        const std::scoped_lock lock(mutex_);
        stream = stream_;
        stream_ = nullptr;
        device_ = 0;
    }

    if (stream != nullptr)
    {
        SDL_DestroyAudioStream(stream);
    }
}

bool Microphone::open()
{
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0)
    {
        spdlog::warn("No microphone: SDL audio is not initialized");
        return false;
    }

    {
        const std::scoped_lock lock(mutex_);

        if (stream_ != nullptr)
        {
            return true;
        }
    }

    SDL_AudioDeviceID device = SDL_AUDIO_DEVICE_DEFAULT_RECORDING;
    const char* openedName = "Windows default";

    if (!deviceName_.empty())
    {
        device = findRecordingDevice(deviceName_);
        openedName = deviceName_.c_str();

        if (device == SDL_AUDIO_DEVICE_DEFAULT_RECORDING)
        {
            spdlog::warn("Microphone '{}' is gone; using the Windows default", deviceName_);
        }
    }

    const SDL_AudioSpec format{
        .format = SDL_AUDIO_S16,
        .channels = static_cast<int>(kMicrophoneChannels),
        .freq = 48000,
    };

    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(device, &format, &captureCallback, this);

    if (stream == nullptr)
    {
        spdlog::warn("Cannot open the microphone '{}': {}", openedName, SDL_GetError());
        return false;
    }

    bool muted = true;

    {
        const std::scoped_lock lock(mutex_);
        stream_ = stream;
        device_ = SDL_GetAudioStreamDevice(stream);
        lost_ = false;
        overflowLogged_ = false;
        buffer_.clear();
        muted = muted_;
    }

    if (muted)
    {
        SDL_PauseAudioStreamDevice(stream);
    }
    else
    {
        SDL_ResumeAudioStreamDevice(stream);
    }

    spdlog::info("Microphone '{}' open at 48 kHz stereo, {}", openedName,
                 muted ? "muted" : "recording");
    return true;
}

void Microphone::close()
{
    SDL_AudioStream* stream = nullptr;

    {
        const std::scoped_lock lock(mutex_);
        stream = stream_;
        stream_ = nullptr;
        device_ = 0;
        buffer_.clear();
    }

    if (stream != nullptr)
    {
        SDL_DestroyAudioStream(stream);
    }
}

void Microphone::setMuted(bool muted)
{
    SDL_AudioStream* stream = nullptr;

    {
        const std::scoped_lock lock(mutex_);

        if (muted_ == muted)
        {
            return;
        }

        muted_ = muted;
        buffer_.clear();
        stream = stream_;
    }

    if (stream != nullptr)
    {
        if (muted)
        {
            SDL_PauseAudioStreamDevice(stream);
        }
        else
        {
            SDL_ResumeAudioStreamDevice(stream);
        }
    }

    condition_.notify_all();
}

bool Microphone::lost() const
{
    const std::scoped_lock lock(mutex_);
    return lost_;
}

void Microphone::setOnLost(std::function<void()> onLost)
{
    const std::scoped_lock lock(mutex_);
    onLost_ = std::move(onLost);
}

void Microphone::onAudioDeviceRemoved(bool recording, std::uint32_t device)
{
    SDL_AudioStream* stream = nullptr;
    std::function<void()> lost;

    {
        const std::scoped_lock lock(mutex_);

        if (!recording || stream_ == nullptr || device != device_)
        {
            return;
        }

        spdlog::warn("Microphone '{}' was removed", deviceName_);
        stream = stream_;
        stream_ = nullptr;
        device_ = 0;
        lost_ = true;
        muted_ = true;
        buffer_.clear();
        lost = onLost_;
    }

    SDL_DestroyAudioStream(stream);

    if (lost)
    {
        lost();
    }
}

void SDLCALL Microphone::captureCallback(void* user, SDL_AudioStream* stream, int additionalAmount,
                                         int totalAmount)
{
    static_cast<void>(additionalAmount);
    static_cast<void>(totalAmount);

    auto& microphone = *static_cast<Microphone*>(user);
    std::array<std::uint8_t, 4096> chunk;
    int available = SDL_GetAudioStreamAvailable(stream);

    while (available > 0)
    {
        const int requested = std::min(available, static_cast<int>(chunk.size()));
        const int received = SDL_GetAudioStreamData(stream, chunk.data(), requested);

        if (received <= 0)
        {
            return;
        }

        microphone.enqueue({chunk.data(), static_cast<std::size_t>(received)});
        available -= received;
    }
}

void Microphone::enqueue(std::span<const std::uint8_t> data)
{
    const std::scoped_lock lock(mutex_);

    if (muted_ || lost_ || stream_ == nullptr || data.empty())
    {
        return;
    }

    if (data.size() >= kBacklogBytes)
    {
        buffer_.clear();
        buffer_.insert(buffer_.end(), data.end() - static_cast<std::ptrdiff_t>(kBacklogBytes),
                       data.end());
    }
    else
    {
        if (buffer_.size() + data.size() > kBacklogBytes)
        {
            const std::size_t overflow = buffer_.size() + data.size() - kBacklogBytes;
            buffer_.erase(buffer_.begin(),
                          buffer_.begin() + static_cast<std::ptrdiff_t>(overflow));

            if (!overflowLogged_)
            {
                spdlog::warn("Microphone fell behind; dropping the oldest captured audio");
                overflowLogged_ = true;
            }
        }

        buffer_.insert(buffer_.end(), data.begin(), data.end());
    }

    condition_.notify_one();
}

void Microphone::drain(std::stop_token stop)
{
    std::array<std::uint8_t, kMicrophoneBlockBytes> block;
    std::array<std::int16_t, kMicrophoneFramesPerBlock * kMicrophoneChannels> samples;

    std::unique_lock lock(mutex_);

    while (true)
    {
        condition_.wait(lock, stop,
                        [this]
                        {
                            return !muted_ && !lost_ &&
                                   buffer_.size() >= kMicrophoneBlockBytes;
                        });

        if (stop.stop_requested())
        {
            return;
        }

        for (std::uint8_t& byte : block)
        {
            byte = buffer_.front();
            buffer_.pop_front();
        }

        std::memcpy(samples.data(), block.data(), block.size());
        lock.unlock();

        if (callback_)
        {
            callback_(samples);
        }

        lock.lock();
    }
}

} // namespace mfplay::audio
