#include "render/TestPattern.h"

#include "render/ColorConversion.h"

#include <algorithm>
#include <cmath>

namespace lynx::render
{

namespace
{

constexpr double kHueCycleSeconds = 12.0;
constexpr double kBarSweepSeconds = 2.0;
constexpr double kBarWidthFraction = 0.04;
constexpr float kSaturation = 0.6F;
constexpr float kValue = 0.35F;
constexpr std::array<float, 3> kWhite{1.0F, 1.0F, 1.0F};

// HSV to RGB, with hue in [0, 1).
std::array<float, 3> hsvToRgb(float hue, float saturation, float value)
{
    const float sector = hue * 6.0F;
    const float chroma = value * saturation;
    const float secondary = chroma * (1.0F - std::abs(std::fmod(sector, 2.0F) - 1.0F));
    const float base = value - chroma;

    const auto rgb = [base](float red, float green, float blue)
    { return std::array{red + base, green + base, blue + base}; };

    switch (static_cast<int>(sector))
    {
    case 0:
        return rgb(chroma, secondary, 0.0F);
    case 1:
        return rgb(secondary, chroma, 0.0F);
    case 2:
        return rgb(0.0F, chroma, secondary);
    case 3:
        return rgb(0.0F, secondary, chroma);
    case 4:
        return rgb(secondary, 0.0F, chroma);
    default:
        return rgb(chroma, 0.0F, secondary);
    }
}

std::uint32_t roundDownToEven(double value)
{
    return static_cast<std::uint32_t>(value) & ~std::uint32_t{1};
}

void fillRow(std::span<std::uint8_t> row, std::size_t first, std::size_t count,
             std::uint8_t value)
{
    std::ranges::fill(row.subspan(first, count), value);
}

// Interleaved Cb and Cr for `pairs` chroma samples from `firstPair`.
void fillChromaRow(std::span<std::uint8_t> row, std::size_t firstPair, std::size_t pairs,
                   Ycbcr8 color)
{
    for (std::size_t pair = firstPair; pair < firstPair + pairs; ++pair)
    {
        row[2 * pair] = color.cb;
        row[2 * pair + 1] = color.cr;
    }
}

} // namespace

TestPattern testPatternAt(double seconds, SurfaceSize picture)
{
    const auto hue = static_cast<float>(std::fmod(seconds, kHueCycleSeconds) / kHueCycleSeconds);
    const double sweep = std::fmod(seconds, kBarSweepSeconds) / kBarSweepSeconds;

    const double width = static_cast<double>(picture.width);
    const std::uint32_t barWidth = std::max(roundDownToEven(width * kBarWidthFraction), 2U);
    const std::uint32_t barLeft = roundDownToEven(sweep * (width - barWidth));

    return {
        .background = hsvToRgb(hue, kSaturation, kValue),
        .bar = {.left = barLeft, .top = 0, .width = barWidth, .height = picture.height},
    };
}

void writeNv12(const TestPattern& pattern, SurfaceSize picture, PlaneView luma,
               PlaneView chroma)
{
    const Ycbcr8 background = rgbToYcbcr8(ColorMatrix::Bt709, pattern.background);
    const Ycbcr8 bar = rgbToYcbcr8(ColorMatrix::Bt709, kWhite);
    const std::size_t barLeft = pattern.bar.left;
    const std::size_t barWidth = std::min<std::size_t>(pattern.bar.width, picture.width - barLeft);
    const std::size_t barRight = barLeft + barWidth;

    for (std::size_t y = 0; y < picture.height; ++y)
    {
        const auto row = luma.bytes.subspan(y * luma.pitch, picture.width);
        fillRow(row, 0, barLeft, background.y);
        fillRow(row, barLeft, barWidth, bar.y);
        fillRow(row, barRight, picture.width - barRight, background.y);
    }

    const std::size_t chromaPairs = picture.width / 2;

    for (std::size_t y = 0; y < picture.height / 2; ++y)
    {
        const auto row = chroma.bytes.subspan(y * chroma.pitch, 2 * chromaPairs);
        fillChromaRow(row, 0, barLeft / 2, background);
        fillChromaRow(row, barLeft / 2, barWidth / 2, bar);
        fillChromaRow(row, barRight / 2, chromaPairs - barRight / 2, background);
    }
}

} // namespace lynx::render
