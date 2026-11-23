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
        .hdr = HdrMode::RtxVideoHdr,
    };
    settings.audio = {
        .outputDevice = "Fone de ouvido (Arctis 7)",
        .microphoneDevice = "Microfone do DualSense",
        .microphoneMuted = true,
        .volumePercent = 70,
    };
    settings.controller.preferred = "DualSense Wireless Controller";
    settings.rtx = {
        .reflex = ReflexMode::Boost,
        .upscaling = Upscaling::Dlss,
        .opticalFlow = false,
        .frameGeneration = FrameGenerationMode::X2,
    };
    settings.overlay.statisticsVisible = true;
    settings.psn.signedIn = true;
    settings.psn.tokenExpiry = 1'792'000'000;
    std::ostringstream output;

    writeSettings(settings, output);
    const auto readBack = read(output.str());

    REQUIRE(readBack.has_value());
    CHECK(readBack->video == settings.video);
    CHECK(readBack->audio == settings.audio);
    CHECK(readBack->controller == settings.controller);
    CHECK(readBack->rtx == settings.rtx);
    // Each RTX group survives on its own, so a field the writer forgot fails
    // here and not only in the whole-struct comparison.
    CHECK(readBack->rtx.reflex == ReflexMode::Boost);
    CHECK(readBack->rtx.upscaling == Upscaling::Dlss);
    CHECK(readBack->rtx.frameGeneration == FrameGenerationMode::X2);
    CHECK_FALSE(readBack->rtx.opticalFlow);
    CHECK(readBack->overlay == settings.overlay);
    CHECK(readBack->psn == settings.psn);
}

TEST_CASE("A settings file without the PSN sign-in starts signed out")
{
    const auto readBack = read(R"({"version":1})");

    REQUIRE(readBack.has_value());
    CHECK_FALSE(readBack->psn.signedIn);
    CHECK_FALSE(readBack->psn.tokenExpiry.has_value());
}

TEST_CASE("The defaults ask for 1080p60 H.265 at the top bitrate with VSync and Reflex")
{
    const Settings defaults;

    CHECK(defaults.video.resolution == VideoResolution::P1080);
    CHECK(defaults.video.framesPerSecond == 60);
    CHECK(defaults.video.codec == VideoCodec::H265);
    CHECK(defaults.video.bitrateKbps == kMaxBitrateKbps);
    CHECK(defaults.video.presentMode == PresentMode::VSync);
    CHECK(defaults.video.hdr == HdrMode::Auto);
    CHECK(defaults.rtx.reflex == ReflexMode::On);
    CHECK(defaults.rtx.upscaling == Upscaling::Shader);
    CHECK(defaults.rtx.opticalFlow);
    CHECK(defaults.rtx.frameGeneration == FrameGenerationMode::Off);
}

TEST_CASE("Audio and controller defaults use the Windows default devices")
{
    const AudioSettings audio;
    const ControllerSettings controller;

    CHECK(audio.outputDevice.empty());
    CHECK(audio.microphoneDevice.empty());
    CHECK_FALSE(audio.microphoneMuted);
    CHECK(audio.volumePercent == 85);
    CHECK(controller.preferred.empty());
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
            "presentMode": true,
            "hdr": "hdr10"
        },
        "audio": {
            "outputDevice": 4,
            "microphoneDevice": null,
            "microphoneMuted": "yes",
            "volumePercent": 140
        },
        "controller": {
            "preferred": false
        },
        "rtx": {
            "reflex": "turbo",
            "upscaling": "turbo",
            "videoSuperResolution": "yes",
            "opticalFlow": "yes",
            "frameGeneration": "4x"
        }
    })");

    REQUIRE(settings.has_value());
    CHECK(settings->video == VideoSettings{});
    CHECK(settings->audio == AudioSettings{});
    CHECK(settings->controller == ControllerSettings{});
    CHECK(settings->rtx == RtxSettings{});
}

TEST_CASE("Settings refuse files that aren't theirs")
{
    CHECK_FALSE(read("not json").has_value());
    CHECK_FALSE(read("[1, 2]").has_value());
    CHECK_FALSE(read(R"({"version": 2, "video": {}})").has_value());
}

TEST_CASE("An old videoSuperResolution setting migrates to the upscaling choice")
{
    const auto withVsr = read(R"({"version": 1, "rtx": {"videoSuperResolution": true}})");
    const auto withShader = read(R"({"version": 1, "rtx": {"videoSuperResolution": false}})");
    const auto withBoth =
        read(R"({"version": 1, "rtx": {"upscaling": "dlss", "videoSuperResolution": true}})");

    REQUIRE(withVsr.has_value());
    REQUIRE(withShader.has_value());
    REQUIRE(withBoth.has_value());
    CHECK(withVsr->rtx.upscaling == Upscaling::Vsr);
    CHECK(withShader->rtx.upscaling == Upscaling::Shader);
    CHECK(withBoth->rtx.upscaling == Upscaling::Dlss);
}
