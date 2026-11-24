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
