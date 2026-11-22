#include "render/ColorConversion.h"

#include <array>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace lynx::render;

namespace
{

// What the shader computes for one sample of each plane.
std::array<float, 3> toRgb(const YuvToRgb& conversion, std::array<float, 3> sample)
{
    std::array<float, 3> rgb{};

    for (std::size_t channel = 0; channel < rgb.size(); ++channel)
    {
        const auto& row = conversion.rows.at(channel);
        rgb.at(channel) = row[0] * sample[0] + row[1] * sample[1] + row[2] * sample[2] + row[3];
    }

    return rgb;
}

std::array<float, 3> eightBit(int y, int cb, int cr)
{
    return {static_cast<float>(y) / 255.0F, static_cast<float>(cb) / 255.0F,
            static_cast<float>(cr) / 255.0F};
}

// A 10-bit code as P010 stores it, read as a 16-bit UNORM sample.
float p010(int code)
{
    return static_cast<float>(code * 64) / 65535.0F;
}

void checkRgb(std::array<float, 3> actual, std::array<float, 3> expected)
{
    for (std::size_t channel = 0; channel < actual.size(); ++channel)
    {
        CHECK(actual.at(channel) == Catch::Approx(expected.at(channel)).margin(0.01));
    }
}

} // namespace

TEST_CASE("BT.709 limited range maps video white and black to the RGB extremes")
{
    const auto conversion = yuvToRgb(ColorMatrix::Bt709, ColorRange::Limited, PixelFormat::Nv12);

    checkRgb(toRgb(conversion, eightBit(235, 128, 128)), {1.0F, 1.0F, 1.0F});
    checkRgb(toRgb(conversion, eightBit(16, 128, 128)), {0.0F, 0.0F, 0.0F});
}

TEST_CASE("BT.709 limited range decodes pure red")
{
    const auto conversion = yuvToRgb(ColorMatrix::Bt709, ColorRange::Limited, PixelFormat::Nv12);

    checkRgb(toRgb(conversion, eightBit(63, 102, 240)), {1.0F, 0.0F, 0.0F});
}

TEST_CASE("BT.601 limited range decodes its own pure red")
{
    const auto bt601 = yuvToRgb(ColorMatrix::Bt601, ColorRange::Limited, PixelFormat::Nv12);

    checkRgb(toRgb(bt601, eightBit(81, 90, 240)), {1.0F, 0.0F, 0.0F});
}

TEST_CASE("Full range uses every code")
{
    const auto conversion = yuvToRgb(ColorMatrix::Bt709, ColorRange::Full, PixelFormat::Nv12);

    checkRgb(toRgb(conversion, eightBit(255, 128, 128)), {1.0F, 1.0F, 1.0F});
    checkRgb(toRgb(conversion, eightBit(0, 128, 128)), {0.0F, 0.0F, 0.0F});
}

TEST_CASE("P010 limited range reads 10-bit codes from the high bits")
{
    const auto conversion = yuvToRgb(ColorMatrix::Bt2020, ColorRange::Limited, PixelFormat::P010);

    checkRgb(toRgb(conversion, {p010(940), p010(512), p010(512)}), {1.0F, 1.0F, 1.0F});
    checkRgb(toRgb(conversion, {p010(64), p010(512), p010(512)}), {0.0F, 0.0F, 0.0F});
}

TEST_CASE("rgbToYcbcr8 gives the BT.709 codes of white, black and red")
{
    CHECK(rgbToYcbcr8(ColorMatrix::Bt709, {1.0F, 1.0F, 1.0F}) == Ycbcr8{235, 128, 128});
    CHECK(rgbToYcbcr8(ColorMatrix::Bt709, {0.0F, 0.0F, 0.0F}) == Ycbcr8{16, 128, 128});
    CHECK(rgbToYcbcr8(ColorMatrix::Bt709, {1.0F, 0.0F, 0.0F}) == Ycbcr8{63, 102, 240});
}

TEST_CASE("A color survives the trip to YCbCr and back")
{
    const std::array<float, 3> teal{0.1F, 0.5F, 0.45F};
    const Ycbcr8 encoded = rgbToYcbcr8(ColorMatrix::Bt709, teal);
    const auto conversion = yuvToRgb(ColorMatrix::Bt709, ColorRange::Limited, PixelFormat::Nv12);

    const auto decoded = toRgb(conversion, eightBit(encoded.y, encoded.cb, encoded.cr));

    for (std::size_t channel = 0; channel < teal.size(); ++channel)
    {
        CHECK(decoded.at(channel) == Catch::Approx(teal.at(channel)).margin(0.01));
    }
}
