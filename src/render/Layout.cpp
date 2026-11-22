#include "render/Layout.h"

namespace lynx::render
{

namespace
{

// `value * numerator / denominator` rounded to the nearest whole number.
std::uint32_t scaleRounded(std::uint32_t value, std::uint32_t numerator,
                           std::uint32_t denominator)
{
    const std::uint64_t product = std::uint64_t{value} * numerator;
    return static_cast<std::uint32_t>((product + denominator / 2) / denominator);
}

} // namespace

PixelRect fitPicture(SurfaceSize surface, SurfaceSize picture)
{
    const bool empty = surface.width == 0 || surface.height == 0 || picture.width == 0 ||
                       picture.height == 0;

    if (empty)
    {
        return {};
    }

    const std::uint64_t surfaceShape = std::uint64_t{surface.width} * picture.height;
    const std::uint64_t pictureShape = std::uint64_t{picture.width} * surface.height;

    if (surfaceShape > pictureShape)
    {
        const std::uint32_t width = scaleRounded(surface.height, picture.width, picture.height);

        return {
            .left = (surface.width - width) / 2,
            .top = 0,
            .width = width,
            .height = surface.height,
        };
    }

    const std::uint32_t height = scaleRounded(surface.width, picture.height, picture.width);

    return {
        .left = 0,
        .top = (surface.height - height) / 2,
        .width = surface.width,
        .height = height,
    };
}

} // namespace lynx::render
