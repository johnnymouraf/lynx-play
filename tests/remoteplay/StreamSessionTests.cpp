#include "remoteplay/StreamSession.h"

#include <catch2/catch_test_macros.hpp>

using namespace lynx;
using remoteplay::VideoProfile;
using remoteplay::videoProfileFor;

TEST_CASE("videoProfileFor asks for 1080p60 H.265 at the chosen bitrate by default")
{
    const VideoProfile fullHd{
        .width = 1920,
        .height = 1080,
        .framesPerSecond = 60,
        .bitrateKbps = core::kMaxBitrateKbps,
        .codec = core::VideoCodec::H265,
    };

    CHECK(videoProfileFor(core::VideoSettings{}) == fullHd);
}

TEST_CASE("videoProfileFor turns each resolution into its 16:9 size")
{
    core::VideoSettings settings;
    settings.framesPerSecond = 30;
    settings.codec = core::VideoCodec::H264;
    settings.bitrateKbps = 6000;

    settings.resolution = core::VideoResolution::P540;
    const VideoProfile small = videoProfileFor(settings);

    settings.resolution = core::VideoResolution::P720;
    const VideoProfile medium = videoProfileFor(settings);

    CHECK(small.width == 960);
    CHECK(small.height == 540);
    CHECK(small.framesPerSecond == 30);
    CHECK(small.codec == core::VideoCodec::H264);
    CHECK(small.bitrateKbps == 6000);
    CHECK(medium.width == 1280);
    CHECK(medium.height == 720);
}
