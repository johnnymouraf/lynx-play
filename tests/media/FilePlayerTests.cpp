#include "media/FilePlayer.h"

#include <catch2/catch_test_macros.hpp>

using lynx::media::codecOfFile;
using lynx::core::VideoCodec;

TEST_CASE("codecOfFile reads the codec from the extension, in any case")
{
    CHECK(codecOfFile("clips/test1080p60.h265") == VideoCodec::H265);
    CHECK(codecOfFile("C:/clips/GAME.HEVC") == VideoCodec::H265);
    CHECK(codecOfFile("test720p60.h264") == VideoCodec::H264);
    CHECK(codecOfFile("capture.264") == VideoCodec::H264);
}

TEST_CASE("codecOfFile knows no container formats")
{
    CHECK_FALSE(codecOfFile("movie.mp4").has_value());
    CHECK_FALSE(codecOfFile("stream").has_value());
}
