#include "core/Settings.h"

#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace lynx::core;

namespace
{

std::expected<Settings, std::string> read(const std::string& text)
{
    std::istringstream input(text);
    return readSettings(input);
}

} // namespace

TEST_CASE("Settings survive a write and a read")
{
    Settings settings;
    settings.video = {
        .resolution = VideoResolution::P720,
        .framesPerSecond = 30,
        .codec = VideoCodec::H264,
        .bitrateKbps = 12000,
        .presentMode = PresentMode::LowestLatency,
    };
    std::ostringstream output;

    writeSettings(settings, output);
    const auto readBack = read(output.str());

    REQUIRE(readBack.has_value());
    CHECK(*readBack == settings);
}

TEST_CASE("The defaults ask for 1080p60 H.265 at the top bitrate with VSync")
{
    const VideoSettings defaults;

    CHECK(defaults.resolution == VideoResolution::P1080);
    CHECK(defaults.framesPerSecond == 60);
    CHECK(defaults.codec == VideoCodec::H265);
    CHECK(defaults.bitrateKbps == kMaxBitrateKbps);
    CHECK(defaults.presentMode == PresentMode::VSync);
}

TEST_CASE("Settings keep their defaults for missing or invalid values")
{
    const auto settings = read(R"({
        "version": 1,
        "video": {
            "resolution": "4k",
            "fps": 45,
            "codec": "h265",
            "bitrateKbps": 900000,
            "presentMode": true
        }
    })");

    REQUIRE(settings.has_value());
    CHECK(settings->video == VideoSettings{});
}

TEST_CASE("Settings refuse files that aren't theirs")
{
    CHECK_FALSE(read("not json").has_value());
    CHECK_FALSE(read("[1, 2]").has_value());
    CHECK_FALSE(read(R"({"version": 2, "video": {}})").has_value());
}
