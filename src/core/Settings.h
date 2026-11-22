#pragma once

#include "core/VideoCodec.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <iosfwd>
#include <string>

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

// What settings.json holds.
struct Settings
{
    VideoSettings video;
    AudioSettings audio;
    ControllerSettings controller;

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
