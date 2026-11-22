#include "render/ReflexSession.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using namespace lynx::render;

TEST_CASE("A sleep is a stall only once ten of the last sixteen exceed the GPU work")
{
    SleepStallDetector detector;

    for (std::size_t index = 0; index + 1 < SleepStallDetector::kThreshold; ++index)
    {
        CHECK_FALSE(detector.observe(10'000, 1'400));
    }

    CHECK(detector.observe(10'000, 1'400));
}

TEST_CASE("Short sleeps never stall, whatever the GPU work")
{
    SleepStallDetector detector;

    for (std::size_t index = 0; index < SleepStallDetector::kWindow; ++index)
    {
        CHECK_FALSE(detector.observe(SleepStallDetector::kFloorUs - 1, 0));
    }
}

TEST_CASE("A sleep shorter than the frame's GPU work is not a stall")
{
    SleepStallDetector detector;

    for (std::size_t index = 0; index < SleepStallDetector::kWindow; ++index)
    {
        CHECK_FALSE(detector.observe(1'000, 2'500));
    }
}

TEST_CASE("The floor makes an unknown GPU work still catch a long sleep")
{
    SleepStallDetector detector;
    bool stalled = false;

    for (std::size_t index = 0; index < SleepStallDetector::kThreshold; ++index)
    {
        stalled = detector.observe(SleepStallDetector::kFloorUs + 1, 0);
    }

    CHECK(stalled);
}

TEST_CASE("Old stalls leave the window")
{
    SleepStallDetector detector;

    for (std::size_t index = 0; index < SleepStallDetector::kThreshold; ++index)
    {
        detector.observe(10'000, 1'000);
    }

    // One full window of short sleeps replaces every long one.
    for (std::size_t index = 0; index < SleepStallDetector::kWindow; ++index)
    {
        detector.observe(1'000, 1'000);
    }

    CHECK_FALSE(detector.observe(1'000, 1'000));
}
