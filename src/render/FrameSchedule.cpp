#include "render/FrameSchedule.h"

namespace mfplay::render
{

FrameSchedule::FrameSchedule(Options options) : options_(options)
{
}

void FrameSchedule::setCadence(std::optional<double> framesPerSecond)
{
    cadence_ = framesPerSecond;
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

    if (!cadence_ || *cadence_ <= 0.0)
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
    return cadence_ && *cadence_ <= options_.repeatedSourceFramesPerSecond;
}

} // namespace mfplay::render
