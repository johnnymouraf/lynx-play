#include "render/Layout.h"

#include <catch2/catch_test_macros.hpp>

using lynx::render::fitPicture;
using lynx::render::PixelRect;
using lynx::render::SurfaceSize;

TEST_CASE("fitPicture fills a surface of the picture's shape")
{
    CHECK(fitPicture({1920, 1080}, {1280, 720}) == PixelRect{0, 0, 1920, 1080});
}

TEST_CASE("fitPicture puts bars left and right on a wider surface")
{
    CHECK(fitPicture({1920, 800}, {1920, 1080}) == PixelRect{249, 0, 1422, 800});
}

TEST_CASE("fitPicture puts bars above and below on a taller surface")
{
    CHECK(fitPicture({1920, 1200}, {1920, 1080}) == PixelRect{0, 60, 1920, 1080});
}

TEST_CASE("fitPicture gives nothing for an empty surface or picture")
{
    CHECK(fitPicture({0, 720}, {1920, 1080}) == PixelRect{});
    CHECK(fitPicture({1280, 720}, {0, 0}) == PixelRect{});
}
