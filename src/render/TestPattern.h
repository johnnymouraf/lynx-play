#pragma once

#include "render/Layout.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace mfplay::render
{

// An animated picture for checking presentation by eye: a background that
// cycles through hues and a white bar that sweeps across it, where stutter or
// tearing is easy to spot.
struct TestPattern
{
    // Gamma-encoded RGB from 0 to 1.
    std::array<float, 3> background{};
    // Starts and spans an even number of columns, so it covers whole chroma
    // samples.
    PixelRect bar{};
};

[[nodiscard]] TestPattern testPatternAt(double seconds, SurfaceSize picture);

// One plane of a picture in memory, `pitch` bytes from one row to the next.
struct PlaneView
{
    std::span<std::uint8_t> bytes;
    std::size_t pitch = 0;
};

// Writes the pattern as NV12 (BT.709, limited range). The picture's width and
// height must be even.
void writeNv12(const TestPattern& pattern, SurfaceSize picture, PlaneView luma,
               PlaneView chroma);

} // namespace mfplay::render
