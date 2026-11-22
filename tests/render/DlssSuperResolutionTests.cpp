#include "render/DlssSuperResolution.h"

#include <catch2/catch_test_macros.hpp>

using lynx::render::dlssArchitectureSupported;
using lynx::render::dlssSuperResolutionEligible;
using lynx::render::kAdaArchitecture;
using lynx::render::SurfaceSize;

TEST_CASE("DLSS SR takes the 1.33x and 2x upscales the window asks for")
{
    CHECK(dlssSuperResolutionEligible({1920, 1080}, {2560, 1440}));
    CHECK(dlssSuperResolutionEligible({1920, 1080}, {3840, 2160}));
    CHECK(dlssSuperResolutionEligible({1280, 720}, {1920, 1080}));
}

TEST_CASE("DLSS SR leaves a 1:1 window and a downscale to VSR")
{
    CHECK_FALSE(dlssSuperResolutionEligible({1920, 1080}, {1920, 1080}));
    CHECK_FALSE(dlssSuperResolutionEligible({1920, 1080}, {1280, 720}));
    CHECK_FALSE(dlssSuperResolutionEligible({1920, 1080}, {1920, 540}));
}

TEST_CASE("DLSS SR refuses more than three times either axis")
{
    CHECK(dlssSuperResolutionEligible({1280, 720}, {3840, 2160}));
    CHECK_FALSE(dlssSuperResolutionEligible({1280, 720}, {4096, 2304}));
    CHECK_FALSE(dlssSuperResolutionEligible({640, 360}, {3840, 2160}));
}

TEST_CASE("DLSS SR needs Ada or newer")
{
    CHECK(dlssArchitectureSupported(kAdaArchitecture));
    CHECK(dlssArchitectureSupported(0x000001B0));
    CHECK_FALSE(dlssArchitectureSupported(0x00000170));
    CHECK_FALSE(dlssArchitectureSupported(0));
}

TEST_CASE("DLSS SR needs a sized picture")
{
    CHECK_FALSE(dlssSuperResolutionEligible({}, {2560, 1440}));
    CHECK_FALSE(dlssSuperResolutionEligible({1920, 1080}, {}));
}
