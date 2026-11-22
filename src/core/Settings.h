#pragma once

#include "core/ReflexMode.h"
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

// The NVIDIA features applied to the stream.
struct RtxSettings
{
    ReflexMode reflex = ReflexMode::On;

    bool operator==(const RtxSettings&) const = default;
};

// What settings.json holds.
struct Settings
{
    VideoSettings video;
    RtxSettings rtx;

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
