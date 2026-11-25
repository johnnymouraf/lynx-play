#include "render/FrameSchedule.h"

#include <utility>

namespace mfplay::render
{

namespace
{

// The display's reported rate is a whole number: 59 Hz stands for 59.94.
constexpr double kRefreshTolerance = 1.03;
// A generated picture and the real one, for each game picture.
constexpr double kGeneratedPresentsPerGamePicture = 2.0;

} // namespace

FrameSchedule::FrameSchedule(Options options) : options_(options)
{
}

void FrameSchedule::setCadence(std::optional<double> framesPerSecond)
{
    cadence_ = framesPerSecond;
}

void FrameSchedule::setDisplay(bool vsync, std::optional<double> refreshHertz)
{
    vsync_ = vsync;
    refreshHertz_ = refreshHertz;
}

bool FrameSchedule::displayTooSlow() const
{
    if (!vsync_ || !refreshHertz_ || *refreshHertz_ <= 0.0 || !cadence_)
    {
        return false;
    }

    return *refreshHertz_ * kRefreshTolerance < kGeneratedPresentsPerGamePicture * *cadence_;
}

void FrameSchedule::reset()
{
    hasHistory_ = false;
    resetPending_ = true;
}

FrameGenerationPlan FrameSchedule::observe(CadenceDetector::Decision decision)
{
    FrameGenerationPlan plan;
    plan.multiFrameCount = options_.multiplier > 0 ? options_.multiplier - 1 : 1;

    if (!cadence_ || *cadence_ <= 0.0 || displayTooSlow())
    {
        hasHistory_ = false;
        resetPending_ = true;
        return plan;
    }

    switch (decision)
    {
    case CadenceDetector::Decision::SceneCut:
        reset();
        return plan;

    case CadenceDetector::Decision::Ignored:
        hasHistory_ = false;
        resetPending_ = true;
        return plan;

    case CadenceDetector::Decision::Repeat:
        // No generation after a repeat; the next distinct picture starts the
        // pair. The real picture still has to reach the screen.
        plan.presentReal = true;
        return plan;

    case CadenceDetector::Decision::Distinct:
        break;
    }

    plan.generate = true;
    plan.reset = resetPending_ || !hasHistory_;
    plan.presentGenerated = true;
    // A repeated source shows the generated picture now and the real one on
    // the repeat; a distinct source shows both, back to back.
    plan.presentReal = !repeatedSource();

    hasHistory_ = true;
    resetPending_ = false;
    return plan;
}

bool FrameSchedule::repeatedSource() const
{
    return cadence_ &&
           *cadence_ <= options_.containerFramesPerSecond * options_.repeatedSourceShare;
}

void HeldRealPicture::hold(Clock::time_point decoded, Clock::time_point now)
{
    decoded_ = decoded;
    heldAt_ = now;
}

bool HeldRealPicture::waiting(Clock::time_point now) const
{
    return decoded_ && now - heldAt_ < kStallAfter;
}

std::optional<HeldRealPicture::Clock::time_point> HeldRealPicture::release()
{
    return std::exchange(decoded_, std::nullopt);
}

} // namespace mfplay::render
