#include "render/TestPattern.h"

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

// HSV to RGB, with hue in [0, 1).
std::array<float, 4> hsvToRgba(float hue, float saturation, float value)
{
    const float sector = hue * 6.0F;
    const float chroma = value * saturation;
    const float secondary = chroma * (1.0F - std::abs(std::fmod(sector, 2.0F) - 1.0F));
    const float base = value - chroma;

    const auto rgba = [base](float red, float green, float blue)
    { return std::array{red + base, green + base, blue + base, 1.0F}; };

    switch (static_cast<int>(sector))
    {
    case 0:
        return rgba(chroma, secondary, 0.0F);
    case 1:
        return rgba(secondary, chroma, 0.0F);
    case 2:
        return rgba(0.0F, chroma, secondary);
    case 3:
        return rgba(0.0F, secondary, chroma);
    case 4:
        return rgba(secondary, 0.0F, chroma);
    default:
        return rgba(chroma, 0.0F, secondary);
    }
}

} // namespace

TestPattern testPatternAt(double seconds, SurfaceSize size)
{
    const auto hue = static_cast<float>(std::fmod(seconds, kHueCycleSeconds) / kHueCycleSeconds);
    const double sweep = std::fmod(seconds, kBarSweepSeconds) / kBarSweepSeconds;

    const double width = static_cast<double>(size.width);
    const auto barWidth = static_cast<LONG>(width * kBarWidthFraction);
    const auto barLeft = static_cast<LONG>(sweep * (width - static_cast<double>(barWidth)));

    return {
        .background = hsvToRgba(hue, kSaturation, kValue),
        .bar = {.left = barLeft,
                .top = 0,
                .right = barLeft + barWidth,
                .bottom = static_cast<LONG>(size.height)},
    };
}

} // namespace lynx::render
