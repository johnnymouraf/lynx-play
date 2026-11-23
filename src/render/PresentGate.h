#pragma once

#include "render/Layout.h"

#include <cstdint>

namespace lynx::render
{

// What a present did with the picture.
enum class PresentOutcome
{
    Presented,
    // DXGI_STATUS_OCCLUDED: the window content is not visible.
    Occluded,
};

// Decides when the swap chain may present. A minimized or hidden window has
// an empty client area, and presenting into it still runs the whole frame; the
// gate keeps the pipeline idle instead. Once the swap chain reports itself
// occluded, the gate retries periodically, since a window can come back
// without a resize to say so.
class PresentGate
{
public:
    // How many frames may be skipped before an occluded chain is tried again.
    static constexpr std::uint32_t kOccludedRetryFrames = 30;

    // The window's client area, read every frame; an empty one hides it.
    void setClientSize(SurfaceSize size);

    // The result of the present the gate let through.
    void observe(PresentOutcome outcome);

    // Whether this frame may render and present. A shown window always may;
    // an empty one never does; an occluded chain gets one retry every
    // kOccludedRetryFrames frames.
    [[nodiscard]] bool mayPresent();

private:
    SurfaceSize clientSize_;
    bool occluded_ = false;
    std::uint32_t skippedSinceRetry_ = 0;
};

} // namespace lynx::render
