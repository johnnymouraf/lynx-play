#pragma once

#include "render/CadenceDetector.h"

#include <chrono>
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
// A source that fills the stream's rate has a distinct picture every frame:
// each pair gets a generated picture and the real one, for twice the presents.
// A source at half the stream's rate repeats every picture once: the distinct
// frame's generated picture takes the first slot and the repeat's real
// picture takes the second, which also keeps the real picture of a pair in
// presentation order. The rates are relative to the stream's, so a 30 fps
// game in a 30 fps stream has no repeats and in a 60 fps stream repeats every
// picture. A repeat never
// generates, and none of this runs until the cadence detector has published
// an estimate from two agreeing windows.
class FrameSchedule
{
public:
    struct Options
    {
        // The rate the stream delivers frames at.
        double containerFramesPerSecond = 60.0;
        // At or below this share of the stream's rate, a distinct picture is
        // followed by repeats. Two thirds sits between a source that fills
        // the stream and the next step down, half of it.
        double repeatedSourceShare = 2.0 / 3.0;
        // Only 2x, the most Ada supports; multiFrameCount is multiplier - 1.
        std::uint32_t multiplier = 2;
    };

    FrameSchedule() = default;
    explicit FrameSchedule(Options options);

    // The detector's published estimate; absent until two windows agree.
    void setCadence(std::optional<double> framesPerSecond);

    // The display's refresh rate and whether presents wait for it. Absent
    // when the system doesn't report one.
    void setDisplay(bool vsync, std::optional<double> refreshHertz);

    // The display refreshes too slowly to show a generated picture and a real
    // one for every game picture, so nothing is generated. Only a waiting
    // present is bound by the refresh; an immediate one is not.
    [[nodiscard]] bool displayTooSlow() const;

    // Forgets the pair history: dropped frames, a resize, a source change or
    // the setting being turned on.
    void reset();

    // Classifies one accepted source frame and says what to present.
    [[nodiscard]] FrameGenerationPlan observe(CadenceDetector::Decision decision);

private:
    [[nodiscard]] bool repeatedSource() const;

    Options options_;
    std::optional<double> cadence_;
    bool vsync_ = false;
    std::optional<double> refreshHertz_;
    bool hasHistory_ = false;
    bool resetPending_ = false;
};

// The real picture a repeated source holds back: the generated picture took
// the distinct frame's slot, and the real one waits for the repeat that
// follows. A redraw must not present it early.
class HeldRealPicture
{
public:
    using Clock = std::chrono::steady_clock;

    // The repeat that should release the picture never arrives when the
    // stream stalls; after this long a redraw may show it.
    static constexpr std::chrono::milliseconds kStallAfter{100};

    // `decoded` is when the distinct frame came out of the decoder.
    void hold(Clock::time_point decoded, Clock::time_point now);

    // A picture is held and the repeat is still due.
    [[nodiscard]] bool waiting(Clock::time_point now) const;

    // The real picture reached the screen; the held frame's decode time.
    std::optional<Clock::time_point> release();

private:
    std::optional<Clock::time_point> decoded_;
    Clock::time_point heldAt_;
};

} // namespace mfplay::render
