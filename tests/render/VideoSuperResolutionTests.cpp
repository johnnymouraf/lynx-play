#include "render/AdapterSelection.h"
#include "render/Layout.h"
#include "render/VideoSuperResolution.h"

#include <catch2/catch_test_macros.hpp>

using mfplay::render::fitPicture;
using mfplay::render::kNvidiaVendorId;
using mfplay::render::PixelRect;
using mfplay::render::SurfaceSize;
using mfplay::render::videoSuperResolutionEligible;

namespace
{

constexpr std::uint32_t kIntelVendorId = 0x8086;

} // namespace

TEST_CASE("VSR runs on an NVIDIA GPU when the output keeps or grows the picture")
{
    CHECK(videoSuperResolutionEligible(true, kNvidiaVendorId, {1280, 720}, {1920, 1080}));
    CHECK(videoSuperResolutionEligible(true, kNvidiaVendorId, {1920, 1080}, {1920, 1080}));
    CHECK(videoSuperResolutionEligible(true, kNvidiaVendorId, {1280, 720}, {1422, 800}));
}

TEST_CASE("VSR stays off without the setting or on another vendor")
{
    CHECK_FALSE(videoSuperResolutionEligible(false, kNvidiaVendorId, {1280, 720}, {1920, 1080}));
    CHECK_FALSE(videoSuperResolutionEligible(true, kIntelVendorId, {1280, 720}, {1920, 1080}));
}

TEST_CASE("VSR leaves a downscale to the shader path")
{
    const SurfaceSize picture{1920, 1080};
    const SurfaceSize window{1280, 720};
    const PixelRect fit = fitPicture(window, picture);

    CHECK_FALSE(videoSuperResolutionEligible(true, kNvidiaVendorId, picture,
                                             {fit.width, fit.height}));
    CHECK_FALSE(videoSuperResolutionEligible(true, kNvidiaVendorId, {1920, 1080}, {1280, 1080}));
    CHECK_FALSE(videoSuperResolutionEligible(true, kNvidiaVendorId, {1920, 1080}, {1920, 540}));
}

TEST_CASE("VSR needs a sized picture")
{
    CHECK_FALSE(videoSuperResolutionEligible(true, kNvidiaVendorId, {}, {1920, 1080}));
    CHECK_FALSE(videoSuperResolutionEligible(true, kNvidiaVendorId, {1280, 720}, {}));
}
