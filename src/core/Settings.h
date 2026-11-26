#pragma once

#include "core/ReflexMode.h"
#include "core/VideoCodec.h"

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

namespace mfplay::core
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

// How the stream window shows.
enum class WindowMode
{
    // Borderless, covering the whole monitor.
    Fullscreen,
    Windowed,
};

[[nodiscard]] constexpr std::string_view windowModeName(WindowMode mode)
{
    switch (mode)
    {
    case WindowMode::Fullscreen:
        return "fullscreen";
    case WindowMode::Windowed:
        return "windowed";
    }

    return "fullscreen";
}

// The stream window's client size, in physical pixels.
struct WindowSize
{
    int width = 1280;
    int height = 720;

    bool operator==(const WindowSize&) const = default;
};

inline constexpr WindowSize kDefaultWindowSize{.width = 1280, .height = 720};
inline constexpr WindowSize kMaxWindowSize{.width = 16384, .height = 16384};

inline constexpr std::array<WindowSize, 4> kWindowSizePresets{{
    {.width = 1920, .height = 1080},
    {.width = 1600, .height = 900},
    {.width = 1280, .height = 720},
    {.width = 960, .height = 540},
}};

inline constexpr std::array<std::string_view, 4> kWindowSizePresetNames{{
    "1920x1080",
    "1600x900",
    "1280x720",
    "960x540",
}};

// How far a client size may be from a preset and still read as one, in pixels
// per dimension: a drag lands a pixel or two off the round numbers.
inline constexpr int kWindowSizePresetTolerance = 2;

// The preset a client size matches, if any.
[[nodiscard]] std::optional<std::size_t> windowSizePresetIndex(WindowSize size);

// The requested size, at least 1×1 and no bigger than the given maximum (a
// monitor's work area).
[[nodiscard]] WindowSize clampWindowSize(WindowSize size, WindowSize maximum);

// A window mode with the windowed size it returns to.
struct WindowLayout
{
    WindowMode mode = WindowMode::Fullscreen;
    WindowSize size = kDefaultWindowSize;

    bool operator==(const WindowLayout&) const = default;
};

// The next layout the F1 menu's row offers: from fullscreen it restores the
// remembered windowed size, or the first preset for a custom one; each preset
// moves to the next, and the last one goes back to fullscreen.
[[nodiscard]] WindowLayout nextWindowLayout(WindowLayout current);

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

// DLSS Frame Generation: one generated picture between each pair of real
// ones, doubling the presented rate when the display allows it.
enum class FrameGenerationMode
{
    Off,
    // 2x: one generated frame per real frame.
    X2,
};

[[nodiscard]] constexpr std::string_view frameGenerationName(FrameGenerationMode mode)
{
    switch (mode)
    {
    case FrameGenerationMode::Off:
        return "off";
    case FrameGenerationMode::X2:
        return "2x";
    }

    return "off";
}

inline constexpr std::uint32_t kMinBitrateKbps = 2000;
inline constexpr std::uint32_t kMaxBitrateKbps = 80000;

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
    // The stream window's presentation; the video profile above is what the
    // PS5 sends either way.
    WindowMode windowMode = WindowMode::Fullscreen;
    // The last windowed client size, which fullscreen returns to.
    WindowSize windowSize = kDefaultWindowSize;

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
    // When off, the keyboard and mouse no longer play the controller.
    bool enabled = true;
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

// How much the in-stream statistics panel shows.
enum class StatisticsMode
{
    Off,
    Simple,
    Advanced,
};

[[nodiscard]] constexpr std::string_view statisticsModeName(StatisticsMode mode)
{
    switch (mode)
    {
    case StatisticsMode::Off:
        return "off";
    case StatisticsMode::Simple:
        return "simple";
    case StatisticsMode::Advanced:
        return "advanced";
    }

    return "off";
}

// The mode F2 switches to: Off, Simple, Advanced, then Off again.
[[nodiscard]] constexpr StatisticsMode nextStatisticsMode(StatisticsMode mode)
{
    switch (mode)
    {
    case StatisticsMode::Off:
        return StatisticsMode::Simple;
    case StatisticsMode::Simple:
        return StatisticsMode::Advanced;
    case StatisticsMode::Advanced:
        return StatisticsMode::Off;
    }

    return StatisticsMode::Off;
}

// What the in-stream overlay shows without being asked.
struct OverlaySettings
{
    StatisticsMode statisticsMode = StatisticsMode::Off;

    bool operator==(const OverlaySettings&) const = default;
};

// How the Consoles screen lists the consoles.
enum class ConsoleView
{
    Grid,
    List,
};

[[nodiscard]] constexpr std::string_view consoleViewName(ConsoleView view)
{
    switch (view)
    {
    case ConsoleView::Grid:
        return "grid";
    case ConsoleView::List:
        return "list";
    }

    return "grid";
}

// What the interface itself remembers between runs.
struct InterfaceSettings
{
    ConsoleView consoleView = ConsoleView::Grid;

    bool operator==(const InterfaceSettings&) const = default;
};

// How the stream is scaled to the window.
enum class Upscaling
{
    // The renderer's own scaling pass.
    Shader,
    // RTX Video Super Resolution through the driver's video processor.
    Vsr,
    // DLSS Super Resolution through NGX, fed by the optical flow.
    Dlss,
};

[[nodiscard]] constexpr std::string_view upscalingName(Upscaling upscaling)
{
    switch (upscaling)
    {
    case Upscaling::Shader:
        return "shader";
    case Upscaling::Vsr:
        return "vsr";
    case Upscaling::Dlss:
        return "dlss";
    }

    return "shader";
}

// The NVIDIA features applied to the stream.
struct RtxSettings
{
    ReflexMode reflex = ReflexMode::On;
    Upscaling upscaling = Upscaling::Shader;
    // The driver's optical flow between frames, for cadence detection and
    // for DLSS's motion vectors.
    bool opticalFlow = true;
    // DLSS Frame Generation; it needs Reflex and the optical flow.
    FrameGenerationMode frameGeneration = FrameGenerationMode::Off;

    bool operator==(const RtxSettings&) const = default;
};

// The PSN sign-in; the tokens themselves live sealed in psn.json.
struct PsnSettings
{
    bool signedIn = false;
    // When the stored access token expires, in seconds since the epoch.
    std::optional<std::int64_t> tokenExpiry;
    // This PC's identifier for PSN; generated once and kept, because the PSN
    // ties its tokens to it. Empty until generated.
    std::string clientDuid;

    bool operator==(const PsnSettings&) const = default;
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
    PsnSettings psn;
    InterfaceSettings interfaceSettings;

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

} // namespace mfplay::core
