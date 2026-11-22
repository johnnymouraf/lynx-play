#include "render/TestPattern.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace lynx::render;

namespace
{

constexpr SurfaceSize kPicture{.width = 8, .height = 4};
// Wider than the picture, as GPU row pitches are.
constexpr std::size_t kPitch = 12;
constexpr std::uint8_t kUntouched = 0xEE;

} // namespace

TEST_CASE("testPatternAt keeps the bar on even columns inside the picture")
{
    for (const double seconds : {0.0, 0.37, 1.0, 1.99, 7.5})
    {
        const TestPattern pattern = testPatternAt(seconds, {.width = 1920, .height = 1080});

        CHECK(pattern.bar.left % 2 == 0);
        CHECK(pattern.bar.width % 2 == 0);
        CHECK(pattern.bar.left + pattern.bar.width <= 1920);
        CHECK(pattern.bar.height == 1080);
    }
}

TEST_CASE("writeNv12 draws a white bar over the background")
{
    const TestPattern pattern{
        .background = {1.0F, 0.0F, 0.0F},
        .bar = {.left = 2, .top = 0, .width = 2, .height = 4},
    };
    std::vector<std::uint8_t> luma(kPitch * kPicture.height, kUntouched);
    std::vector<std::uint8_t> chroma(kPitch * kPicture.height / 2, kUntouched);

    writeNv12(pattern, kPicture, {.bytes = luma, .pitch = kPitch},
              {.bytes = chroma, .pitch = kPitch});

    const std::vector<std::uint8_t> secondLumaRow(luma.data() + kPitch, luma.data() + 2 * kPitch);
    CHECK(secondLumaRow == std::vector<std::uint8_t>{63, 63, 235, 235, 63, 63, 63, 63, kUntouched,
                                                     kUntouched, kUntouched, kUntouched});

    const std::vector<std::uint8_t> chromaRow(chroma.data(), chroma.data() + kPitch);
    CHECK(chromaRow == std::vector<std::uint8_t>{102, 240, 128, 128, 102, 240, 102, 240,
                                                 kUntouched, kUntouched, kUntouched, kUntouched});
}
