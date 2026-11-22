#pragma once

#include "render/SwapChain.h"

#include <array>

#include <d3d12.h>

namespace lynx::render
{

// An animated frame for checking presentation by eye: a background that cycles
// through hues and a white bar that sweeps across the screen, where stutter or
// tearing is easy to spot.
struct TestPattern
{
    std::array<float, 4> background{};
    D3D12_RECT bar{};
};

[[nodiscard]] TestPattern testPatternAt(double seconds, SurfaceSize size);

} // namespace lynx::render
