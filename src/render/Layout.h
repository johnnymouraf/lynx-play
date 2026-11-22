#pragma once

#include <cstdint>

namespace lynx::render
{

struct SurfaceSize
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    bool operator==(const SurfaceSize&) const = default;
};

// A rectangle in whole pixels.
struct PixelRect
{
    std::uint32_t left = 0;
    std::uint32_t top = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    bool operator==(const PixelRect&) const = default;
};

// The largest rectangle with the picture's aspect ratio, centered in the
// surface: black bars fill the rest. Empty when either size is.
[[nodiscard]] PixelRect fitPicture(SurfaceSize surface, SurfaceSize picture);

} // namespace lynx::render
