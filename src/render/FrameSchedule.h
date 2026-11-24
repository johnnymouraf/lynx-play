#pragma once

#include "render/CadenceDetector.h"

#include <cstdint>
#include <optional>

namespace mfplay::render
{

// What one accepted source frame does to the presented pictures.
struct FrameGenerationPlan
{
    // Runs DLSS-G for this frame's pair of distinct pictures.
    bool generate = false;
    // Shows the generated picture; it always comes before the real one.
    bool presentGenerated = false;
    // Shows the real picture in this frame. A repeated source holds it back
    // to the repeat that follows, so the generated picture fills this slot.
    bool presentReal = true;
    // Tells DLSS-G its previous picture is unrelated: the first pair, a scene
    // cut, a resize, a dropped frame or a source change.
    bool reset = false;
    // DLSS-G's 2x call: one intermediate frame, index one.
    std::uint32_t multiFrameCount = 1;
    std::uint32_t multiFrameIndex = 1;
};

// Decides which source frames get a generated picture, from the published
// cadence and each frame's distinct/repeated decision.
//
// A 60 fps source has a distinct picture every frame: each pair gets a
// generated picture and the real one, for twice the presents. A 30 fps
// source repeats every picture once: the distinct frame's generated picture
// takes the first slot and the repeat's real picture takes the second, which
// also keeps the real picture of a pair in presentation order. A repeat never
// generates, and none of this runs until the cadence detector has published
// an estimate from two agreeing windows.
class FrameSchedule
{
public:
    struct Options
    {
        // At or below this rate, a distinct picture is followed by repeats.
        double repeatedSourceFramesPerSecond = 40.0;
        // Only 2x, the most Ada supports; multiFrameCount is multiplier - 1.
        std::uint32_t multiplier = 2;
    };

    FrameSchedule() = default;
    explicit FrameSchedule(Options options);

    // The detector's published estimate; absent until two windows agree.
    void setCadence(std::optional<double> framesPerSecond);

    // Forgets the pair history: dropped frames, a resize, a source change or
    // the setting being turned on.
    void reset();

    // Classifies one accepted source frame and says what to present.
    [[nodiscard]] FrameGenerationPlan observe(CadenceDetector::Decision decision);

private:
    [[nodiscard]] bool repeatedSource() const;

    Options options_;
    std::optional<double> cadence_;
    bool hasHistory_ = false;
    bool resetPending_ = false;
};

} // namespace mfplay::render
