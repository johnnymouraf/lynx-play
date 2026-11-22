#pragma once

#include "core/ReflexMode.h"
#include "core/VideoCodec.h"

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>

namespace lynx::core
{

inline constexpr int kSettingsVersion = 1;

enum class VideoResolution
{
    P360,
    P540,
    P720,
    P1080,
};

enum class PresentMode
{
    // No tearing, up to one display refresh of added latency.
    VSync,
    // Each frame shows at once, tearing where the display allows it.
    LowestLatency,
};

// What is asked of the PS5's HDR and what the renderer does with it.
enum class HdrMode
{
    // SDR from end to end.
    Off,
    // Ask the PS5 for HDR; present HDR on an HDR display and tone-map it on an
    // SDR one.
    Auto,
    // Ask the PS5 for SDR and let RTX Video HDR convert it on an HDR display.
    RtxVideoHdr,
};

[[nodiscard]] constexpr std::string_view hdrModeName(HdrMode mode)
{
    switch (mode)
    {
    case HdrMode::Off:
        return "off";
    case HdrMode::Auto:
        return "auto";
    case HdrMode::RtxVideoHdr:
        return "rtx";
    }

    return "auto";
}

inline constexpr std::uint32_t kMinBitrateKbps = 2000;
inline constexpr std::uint32_t kMaxBitrateKbps = 50000;

// What the PS5 is asked to stream and how it is shown.
struct VideoSettings
{
    VideoResolution resolution = VideoResolution::P1080;
    // 30 or 60.
    int framesPerSecond = 60;
    VideoCodec codec = VideoCodec::H265;
    std::uint32_t bitrateKbps = kMaxBitrateKbps;
    PresentMode presentMode = PresentMode::VSync;
    HdrMode hdr = HdrMode::Auto;

    bool operator==(const VideoSettings&) const = default;
};

// The game's sound and the microphone. Empty device names mean the Windows
// default.
struct AudioSettings
{
    std::string outputDevice;
    std::string microphoneDevice;
    bool microphoneMuted = false;
    int volumePercent = 85;

    bool operator==(const AudioSettings&) const = default;
};

struct ControllerSettings
{
    // The controller to use when more than one is connected; empty means the
    // first one.
    std::string preferred;

    bool operator==(const ControllerSettings&) const = default;
};

// The controls a key or mouse control can stand in for.
enum class KeyboardControl
{
    LeftStickUp = 0,
    LeftStickDown,
    LeftStickLeft,
    LeftStickRight,
    RightStick,
    Cross,
    Circle,
    Square,
    Triangle,
    L1,
    R1,
    L2,
    R2,
    Options,
    Create,
    Count,
};

inline constexpr std::size_t kKeyboardControlCount =
    static_cast<std::size_t>(KeyboardControl::Count);

inline constexpr std::array<std::string_view, kKeyboardControlCount> kKeyboardControlNames{{
    "leftStickUp",
    "leftStickDown",
    "leftStickLeft",
    "leftStickRight",
    "rightStick",
    "cross",
    "circle",
    "square",
    "triangle",
    "l1",
    "r1",
    "l2",
    "r2",
    "options",
    "create",
}};

inline constexpr double kMinMouseSensitivity = 0.1;
inline constexpr double kMaxMouseSensitivity = 5.0;

// The design's keyboard and mouse defaults: WASD and the mouse for the
// sticks, then one key per button.
[[nodiscard]] std::array<std::string, kKeyboardControlCount> defaultKeyboardBindings();

struct KeyboardMouseSettings
{
    std::array<std::string, kKeyboardControlCount> bindings = defaultKeyboardBindings();
    double mouseSensitivity = 1.0;
    bool invertY = false;

    bool operator==(const KeyboardMouseSettings&) const = default;
};

// Points a control at a binding; a binding another control already had moves
// to the control that lost it, so two controls never share one.
[[nodiscard]] KeyboardMouseSettings assignKeyboardBinding(KeyboardMouseSettings settings,
                                                          KeyboardControl control,
                                                          std::string_view binding);

// What the in-stream overlay shows without being asked.
struct OverlaySettings
{
    bool statisticsVisible = false;

    bool operator==(const OverlaySettings&) const = default;
};

// The NVIDIA features applied to the stream.
struct RtxSettings
{
    ReflexMode reflex = ReflexMode::On;
    // RTX Video Super Resolution through the driver's video processor.
    bool videoSuperResolution = false;
    // The driver's optical flow between frames, for cadence detection now and
    // DLSS later.
    bool opticalFlow = true;

    bool operator==(const RtxSettings&) const = default;
};

// What settings.json holds.
struct Settings
{
    VideoSettings video;
    AudioSettings audio;
    ControllerSettings controller;
    RtxSettings rtx;
    OverlaySettings overlay;
    KeyboardMouseSettings keyboardMouse;

    bool operator==(const Settings&) const = default;
};

// Reads settings.json. A value that is missing or out of range keeps its
// default; an error means the input isn't a settings file this version reads.
[[nodiscard]] std::expected<Settings, std::string> readSettings(std::istream& input);

void writeSettings(const Settings& settings, std::ostream& output);

// settings.json on disk.
class SettingsStore
{
public:
    explicit SettingsStore(std::filesystem::path path);

    // A missing or unreadable file gives the defaults.
    [[nodiscard]] Settings load() const;

    // Replaces the file through a temporary one, so a failure leaves the old
    // file whole.
    [[nodiscard]] std::expected<void, std::string> save(const Settings& settings) const;

private:
    std::filesystem::path path_;
};

} // namespace lynx::core
