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
        .hdr = true,
    };

    CHECK(videoProfileFor(core::VideoSettings{}) == fullHd);
}

TEST_CASE("HDR is asked for only with the automatic setting, H.265 and a PS5")
{
    core::VideoSettings settings;

    settings.hdr = core::HdrMode::Off;
    CHECK_FALSE(videoProfileFor(settings).hdr);

    settings.hdr = core::HdrMode::RtxVideoHdr;
    CHECK_FALSE(videoProfileFor(settings).hdr);

    settings.hdr = core::HdrMode::Auto;
    CHECK(videoProfileFor(settings).hdr);
    CHECK_FALSE(videoProfileFor(settings, false).hdr);

    settings.codec = core::VideoCodec::H264;
    CHECK_FALSE(videoProfileFor(settings).hdr);
    CHECK_FALSE(videoProfileFor(settings, false).hdr);
}

TEST_CASE("videoProfileFor turns each resolution into its 16:9 size")
{
    core::VideoSettings settings;
    settings.framesPerSecond = 30;
    settings.codec = core::VideoCodec::H264;
    settings.bitrateKbps = 6000;

    settings.resolution = core::VideoResolution::P540;
    const VideoProfile smallProfile = videoProfileFor(settings);

    settings.resolution = core::VideoResolution::P720;
    const VideoProfile mediumProfile = videoProfileFor(settings);

    CHECK(smallProfile.width == 960);
    CHECK(smallProfile.height == 540);
    CHECK(smallProfile.framesPerSecond == 30);
    CHECK(smallProfile.codec == core::VideoCodec::H264);
    CHECK(smallProfile.bitrateKbps == 6000);
    CHECK(mediumProfile.width == 1280);
    CHECK(mediumProfile.height == 720);
}
