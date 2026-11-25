#include "render/FrameSchedule.h"

#include <catch2/catch_test_macros.hpp>

using namespace mfplay::render;
using Decision = CadenceDetector::Decision;

TEST_CASE("No published cadence keeps every frame real and generated off")
{
    FrameSchedule schedule;

    for (int frame = 0; frame < 4; ++frame)
    {
        const FrameGenerationPlan plan = schedule.observe(Decision::Distinct);

        CHECK_FALSE(plan.generate);
        CHECK_FALSE(plan.presentGenerated);
        CHECK(plan.presentReal);
    }
}

TEST_CASE("A distinct 60 fps source generates one frame per pair")
{
    FrameSchedule schedule;
    schedule.setCadence(60.0);

    const FrameGenerationPlan first = schedule.observe(Decision::Distinct);

    CHECK(first.generate);
    CHECK(first.presentGenerated);
    CHECK(first.presentReal);
    CHECK(first.reset);
    CHECK(first.multiFrameCount == 1);
    CHECK(first.multiFrameIndex == 1);

    const FrameGenerationPlan second = schedule.observe(Decision::Distinct);

    CHECK(second.generate);
    CHECK(second.presentGenerated);
    CHECK(second.presentReal);
    CHECK_FALSE(second.reset);
}

TEST_CASE("A 30 fps source presents the generated picture and keeps the real one for the repeat")
{
    FrameSchedule schedule;
    schedule.setCadence(30.0);

    const FrameGenerationPlan distinct = schedule.observe(Decision::Distinct);

    CHECK(distinct.generate);
    CHECK(distinct.presentGenerated);
    CHECK_FALSE(distinct.presentReal);
    CHECK(distinct.multiFrameCount == 1);

    const FrameGenerationPlan repeat = schedule.observe(Decision::Repeat);

    CHECK_FALSE(repeat.generate);
    CHECK_FALSE(repeat.presentGenerated);
    CHECK(repeat.presentReal);
}

TEST_CASE("A repeat pauses generation and shows its real picture")
{
    FrameSchedule schedule;
    schedule.setCadence(60.0);
    (void)schedule.observe(Decision::Distinct);

    const FrameGenerationPlan repeat = schedule.observe(Decision::Repeat);

    CHECK_FALSE(repeat.generate);
    CHECK_FALSE(repeat.presentGenerated);
    CHECK(repeat.presentReal);
}

TEST_CASE("A scene cut resets the next pair")
{
    FrameSchedule schedule;
    schedule.setCadence(60.0);
    (void)schedule.observe(Decision::Distinct);
    (void)schedule.observe(Decision::Distinct);

    const FrameGenerationPlan cut = schedule.observe(Decision::SceneCut);

    CHECK_FALSE(cut.generate);
    CHECK(cut.presentReal);

    const FrameGenerationPlan next = schedule.observe(Decision::Distinct);

    CHECK(next.generate);
    CHECK(next.reset);
}

TEST_CASE("An unmeasured frame resets the pair")
{
    FrameSchedule schedule;
    schedule.setCadence(60.0);
    (void)schedule.observe(Decision::Distinct);

    const FrameGenerationPlan ignored = schedule.observe(Decision::Ignored);

    CHECK_FALSE(ignored.generate);
    CHECK(ignored.presentReal);

    const FrameGenerationPlan next = schedule.observe(Decision::Distinct);

    CHECK(next.generate);
    CHECK(next.reset);
}

TEST_CASE("Reset makes the next pair start over")
{
    FrameSchedule schedule;
    schedule.setCadence(60.0);
    (void)schedule.observe(Decision::Distinct);

    schedule.reset();

    const FrameGenerationPlan next = schedule.observe(Decision::Distinct);

    CHECK(next.generate);
    CHECK(next.reset);
}

TEST_CASE("Twenty-four frames per second in a sixty-hertz container holds the real picture")
{
    FrameSchedule schedule;
    schedule.setCadence(24.0);

    const FrameGenerationPlan distinct = schedule.observe(Decision::Distinct);

    CHECK(distinct.generate);
    CHECK(distinct.presentGenerated);
    CHECK_FALSE(distinct.presentReal);
}

TEST_CASE("The multiplier is the 2x pair Ada allows")
{
    FrameSchedule schedule;

    CHECK(FrameSchedule::Options{}.multiplier == 2);
    CHECK(schedule.observe(Decision::Distinct).multiFrameCount == 1);

    FrameSchedule multi(FrameSchedule::Options{.multiplier = 3});
    multi.setCadence(60.0);

    CHECK(multi.observe(Decision::Distinct).multiFrameCount == 2);
}

TEST_CASE("A 30 fps source in a 30 fps stream generates one frame per pair and shows both")
{
    FrameSchedule schedule({.containerFramesPerSecond = 30.0});
    schedule.setCadence(30.0);

    const FrameGenerationPlan plan = schedule.observe(Decision::Distinct);

    CHECK(plan.generate);
    CHECK(plan.presentGenerated);
    CHECK(plan.presentReal);
}

TEST_CASE("A 15 fps source in a 30 fps stream holds the real picture for the repeat")
{
    FrameSchedule schedule({.containerFramesPerSecond = 30.0});
    schedule.setCadence(15.0);

    const FrameGenerationPlan plan = schedule.observe(Decision::Distinct);

    CHECK(plan.generate);
    CHECK(plan.presentGenerated);
    CHECK_FALSE(plan.presentReal);
}

TEST_CASE("A display below twice the game rate generates nothing under VSync")
{
    FrameSchedule schedule;
    schedule.setCadence(60.0);
    schedule.setDisplay(true, 60.0);

    CHECK(schedule.displayTooSlow());

    const FrameGenerationPlan plan = schedule.observe(Decision::Distinct);

    CHECK_FALSE(plan.generate);
    CHECK_FALSE(plan.presentGenerated);
    CHECK(plan.presentReal);
}

TEST_CASE("A 60 Hz display fits a 30 fps game, and the reported 59 Hz still does")
{
    FrameSchedule schedule({.containerFramesPerSecond = 60.0});
    schedule.setCadence(30.0);

    schedule.setDisplay(true, 60.0);
    CHECK_FALSE(schedule.displayTooSlow());

    schedule.setDisplay(true, 59.0);
    CHECK_FALSE(schedule.displayTooSlow());

    schedule.setDisplay(true, 50.0);
    CHECK(schedule.displayTooSlow());
}

TEST_CASE("The refresh guard needs a waiting present, a known refresh rate and a cadence")
{
    FrameSchedule schedule;

    schedule.setDisplay(true, 30.0);
    CHECK_FALSE(schedule.displayTooSlow());

    schedule.setCadence(60.0);

    schedule.setDisplay(false, 30.0);
    CHECK_FALSE(schedule.displayTooSlow());

    schedule.setDisplay(true, std::nullopt);
    CHECK_FALSE(schedule.displayTooSlow());
}

TEST_CASE("Generation resumes with a reset once the display is fast enough")
{
    FrameSchedule schedule;
    schedule.setCadence(60.0);
    schedule.setDisplay(true, 60.0);
    (void)schedule.observe(Decision::Distinct);

    schedule.setDisplay(true, 144.0);
    const FrameGenerationPlan plan = schedule.observe(Decision::Distinct);

    CHECK(plan.generate);
    CHECK(plan.reset);
}

TEST_CASE("A held real picture waits for its repeat until the stream stalls")
{
    using Clock = HeldRealPicture::Clock;
    const Clock::time_point start{};
    HeldRealPicture held;

    CHECK_FALSE(held.waiting(start));

    held.hold(start - std::chrono::milliseconds(2), start);

    CHECK(held.waiting(start + std::chrono::milliseconds(16)));
    CHECK_FALSE(held.waiting(start + HeldRealPicture::kStallAfter));
}

TEST_CASE("Releasing a held real picture gives back the distinct frame's decode time")
{
    using Clock = HeldRealPicture::Clock;
    const Clock::time_point decoded{};
    HeldRealPicture held;
    held.hold(decoded, decoded + std::chrono::milliseconds(3));

    CHECK(held.release() == decoded);
    CHECK_FALSE(held.release().has_value());
    CHECK_FALSE(held.waiting(decoded + std::chrono::milliseconds(4)));
}
