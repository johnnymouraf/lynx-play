#include "render/PresentGate.h"

namespace mfplay::render
{

namespace
{

bool empty(SurfaceSize size)
{
    return size.width == 0 || size.height == 0;
}

} // namespace

void PresentGate::setClientSize(SurfaceSize size)
{
    const bool wasHidden = empty(clientSize_);
    const bool hidden = empty(size);
    clientSize_ = size;

    if (wasHidden && !hidden)
    {
        // A window that comes back, from minimized or hidden, gets a fresh
        // chance to present; without this it would stay parked.
        occluded_ = false;
        skippedSinceRetry_ = 0;
    }
}

void PresentGate::observe(PresentOutcome outcome)
{
    occluded_ = outcome == PresentOutcome::Occluded;
    skippedSinceRetry_ = 0;
}

bool PresentGate::mayPresent()
{
    if (empty(clientSize_))
    {
        return false;
    }

    if (!occluded_)
    {
        return true;
    }

    if (skippedSinceRetry_ + 1 < kOccludedRetryFrames)
    {
        ++skippedSinceRetry_;
        return false;
    }

    skippedSinceRetry_ = 0;
    return true;
}

} // namespace mfplay::render
