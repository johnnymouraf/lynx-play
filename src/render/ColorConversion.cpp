#include "render/ColorConversion.h"

#include <algorithm>
#include <cmath>

namespace lynx::render
{

namespace
{

// The luma weights of red and blue; green takes the rest.
struct LumaWeights
{
    double red = 0.0;
    double blue = 0.0;
};

LumaWeights weightsOf(ColorMatrix matrix)
{
    switch (matrix)
    {
    case ColorMatrix::Bt601:
        return {.red = 0.299, .blue = 0.114};
    case ColorMatrix::Bt709:
        return {.red = 0.2126, .blue = 0.0722};
    case ColorMatrix::Bt2020:
        return {.red = 0.2627, .blue = 0.0593};
    }

    return {.red = 0.2126, .blue = 0.0722};
}

// Turns a normalized texture sample into the integer code it stores.
struct SampleCoding
{
    double toCode = 0.0;
    int bitDepth = 0;
};

SampleCoding codingOf(PixelFormat format)
{
    if (format == PixelFormat::P010)
    {
        // A 10-bit code in the high bits of a 16-bit UNORM sample.
        return {.toCode = 65535.0 / 64.0, .bitDepth = 10};
    }

    return {.toCode = 255.0, .bitDepth = 8};
}

// value = scale * sample + offset, for luma from 0 to 1 or chroma from -0.5
// to 0.5.
struct Expansion
{
    double scale = 0.0;
    double offset = 0.0;
};

struct PlaneExpansions
{
    Expansion luma;
    Expansion chroma;
};

PlaneExpansions expansionsOf(ColorRange range, PixelFormat format)
{
    const SampleCoding coding = codingOf(format);
    const double step = std::ldexp(1.0, coding.bitDepth - 8);
    const double chromaZero = 128.0 * step;

    if (range == ColorRange::Full)
    {
        const double maxCode = std::ldexp(1.0, coding.bitDepth) - 1.0;

        return {
            .luma = {.scale = coding.toCode / maxCode, .offset = 0.0},
            .chroma = {.scale = coding.toCode / maxCode, .offset = -chromaZero / maxCode},
        };
    }

    const double lumaSpan = 219.0 * step;
    const double chromaSpan = 224.0 * step;

    return {
        .luma = {.scale = coding.toCode / lumaSpan, .offset = -16.0 * step / lumaSpan},
        .chroma = {.scale = coding.toCode / chromaSpan, .offset = -chromaZero / chromaSpan},
    };
}

std::array<float, 4> row(const PlaneExpansions& expansions, double fromCb, double fromCr)
{
    const Expansion& luma = expansions.luma;
    const Expansion& chroma = expansions.chroma;
    const double offset = luma.offset + (fromCb + fromCr) * chroma.offset;

    return {
        static_cast<float>(luma.scale),
        static_cast<float>(fromCb * chroma.scale),
        static_cast<float>(fromCr * chroma.scale),
        static_cast<float>(offset),
    };
}

std::uint8_t roundToByte(double value)
{
    return static_cast<std::uint8_t>(std::clamp(std::lround(value), 0L, 255L));
}

} // namespace

YuvToRgb yuvToRgb(ColorMatrix matrix, ColorRange range, PixelFormat format)
{
    const LumaWeights weights = weightsOf(matrix);
    const double green = 1.0 - weights.red - weights.blue;
    const PlaneExpansions expansions = expansionsOf(range, format);

    const double redFromCr = 2.0 * (1.0 - weights.red);
    const double blueFromCb = 2.0 * (1.0 - weights.blue);
    const double greenFromCb = -blueFromCb * weights.blue / green;
    const double greenFromCr = -redFromCr * weights.red / green;

    return {.rows = {
                row(expansions, 0.0, redFromCr),
                row(expansions, greenFromCb, greenFromCr),
                row(expansions, blueFromCb, 0.0),
            }};
}

Ycbcr8 rgbToYcbcr8(ColorMatrix matrix, std::array<float, 3> rgb)
{
    const LumaWeights weights = weightsOf(matrix);
    const double green = 1.0 - weights.red - weights.blue;
    const auto [red, greenValue, blue] = rgb;

    const double luma = weights.red * red + green * greenValue + weights.blue * blue;
    const double cb = (blue - luma) / (2.0 * (1.0 - weights.blue));
    const double cr = (red - luma) / (2.0 * (1.0 - weights.red));

    return {
        .y = roundToByte(16.0 + 219.0 * luma),
        .cb = roundToByte(128.0 + 224.0 * cb),
        .cr = roundToByte(128.0 + 224.0 * cr),
    };
}

} // namespace lynx::render
