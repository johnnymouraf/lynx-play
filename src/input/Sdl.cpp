#include "input/Sdl.h"

#include <utility>

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

namespace lynx::input
{

namespace
{

void setHints()
{
    // Input keeps arriving while the stream window has the focus.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    // About 10 ms of audio per WASAPI buffer on the device.
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "480");
    // Skip Windows' audio effects on the game's sound. A driver that rejects
    // it logs and keeps working.
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_RAW_STREAM, "1");
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_STREAM_NAME, "Lynx Play");
}

} // namespace

std::expected<SdlRuntime, std::string> SdlRuntime::create()
{
    setHints();

    if (!SDL_Init(SDL_INIT_GAMEPAD))
    {
        return std::unexpected(std::string("SDL gamepads did not start: ") + SDL_GetError());
    }

    bool audioReady = true;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        spdlog::warn("SDL audio did not start: {}", SDL_GetError());
        audioReady = false;
    }

    const char* videoDriver = SDL_GetCurrentVideoDriver();
    spdlog::info("SDL {} initialized (video driver: {})", SDL_GetRevision(),
                 videoDriver != nullptr ? videoDriver : "none");
    return SdlRuntime(true, audioReady);
}

SdlRuntime::SdlRuntime(bool gamepadsReady, bool audioReady)
    : gamepadsReady_(gamepadsReady), audioReady_(audioReady)
{
}

SdlRuntime::SdlRuntime(SdlRuntime&& other) noexcept
    : gamepadsReady_(std::exchange(other.gamepadsReady_, false)),
      audioReady_(std::exchange(other.audioReady_, false)),
      active_(std::exchange(other.active_, false))
{
}

SdlRuntime::~SdlRuntime()
{
    if (active_)
    {
        SDL_Quit();
    }
}

bool SdlRuntime::gamepadsReady() const
{
    return gamepadsReady_;
}

bool SdlRuntime::audioReady() const
{
    return audioReady_;
}

} // namespace lynx::input
