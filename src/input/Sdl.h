#pragma once

#include <expected>
#include <string>

namespace lynx::input
{

// SDL's gamepad and audio subsystems for the app's lifetime. Create it
// before the controllers and before any audio device is opened.
class SdlRuntime
{
public:
    // The gamepad subsystem is required; audio failing is only a warning.
    [[nodiscard]] static std::expected<SdlRuntime, std::string> create();

    SdlRuntime(const SdlRuntime&) = delete;
    SdlRuntime& operator=(const SdlRuntime&) = delete;
    SdlRuntime(SdlRuntime&& other) noexcept;
    SdlRuntime& operator=(SdlRuntime&&) = delete;

    ~SdlRuntime();

    [[nodiscard]] bool gamepadsReady() const;
    [[nodiscard]] bool audioReady() const;

private:
    SdlRuntime(bool gamepadsReady, bool audioReady);

    bool gamepadsReady_ = false;
    bool audioReady_ = false;
    bool active_ = true;
};

} // namespace lynx::input
