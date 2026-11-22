#pragma once

#include "render/VideoFrame.h"

#include <array>
#include <cstdint>

namespace lynx::render
{

// Rows that turn the normalized samples a shader reads from a YUV texture,
// (Y, Cb, Cr, 1), into gamma-encoded RGB from 0 to 1.
struct YuvToRgb
{
    std::array<std::array<float, 4>, 3> rows{};
};

[[nodiscard]] YuvToRgb yuvToRgb(ColorMatrix matrix, ColorRange range, PixelFormat format);

// An 8-bit limited-range YCbCr color.
struct Ycbcr8
{
    std::uint8_t y = 0;
    std::uint8_t cb = 0;
    std::uint8_t cr = 0;

    bool operator==(const Ycbcr8&) const = default;
};

// Gamma-encoded RGB from 0 to 1 as 8-bit limited-range YCbCr with `matrix`.
[[nodiscard]] Ycbcr8 rgbToYcbcr8(ColorMatrix matrix, std::array<float, 3> rgb);

} // namespace lynx::render
