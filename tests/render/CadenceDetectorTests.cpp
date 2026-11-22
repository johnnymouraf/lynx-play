#include "render/CadenceDetector.h"

#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace lynx::render;

namespace
{

CadenceSample frame(std::uint32_t changedBlocks, std::uint32_t totalBlocks,
                    double meanDifference)
{
    return {
        .changedBlocks = changedBlocks,
        .totalBlocks = totalBlocks,
        .meanDifference = meanDifference,
    };
}

CadenceSample distinctFrame()
{
    return frame(500, 1'000, 40.0);
}

CadenceSample repeatedFrame()
{
    return frame(0, 1'000, 0.0);
}

void feed(CadenceDetector& detector, std::size_t distinct, std::size_t repeats)
{
    for (std::size_t index = 0; index < distinct; ++index)
    {
        detector.observe(distinctFrame());
    }

    for (std::size_t index = 0; index < repeats; ++index)
    {
        detector.observe(repeatedFrame());
    }
}

} // namespace

TEST_CASE("Under one percent of changed blocks is a repeated frame")
{
    CadenceDetector detector;

    CHECK(detector.observe(frame(9, 1'000, 0.0)) == CadenceDetector::Decision::Repeat);
    CHECK(detector.observe(frame(10, 1'000, 0.0)) == CadenceDetector::Decision::Distinct);
    CHECK(detector.observe(frame(500, 0, 0.0)) == CadenceDetector::Decision::Ignored);
}

TEST_CASE("A scene cut needs most of the picture and a large difference")
{
    CadenceDetector detector;

    CHECK(detector.observe(frame(601, 1'000, 10.0)) == CadenceDetector::Decision::Distinct);
    CHECK(detector.observe(frame(601, 1'000, 40.0)) == CadenceDetector::Decision::SceneCut);
    CHECK(detector.windowFrames() == 0);
}

TEST_CASE("Sixty frames per second publishes after two full windows")
{
    CadenceDetector detector;

    feed(detector, 60, 0);
    CHECK_FALSE(detector.estimate().has_value());

    feed(detector, 60, 0);
    REQUIRE(detector.estimate().has_value());
    CHECK(detector.estimate()->framesPerSecond == Catch::Approx(60.0));
    CHECK(detector.estimate()->repeatedFrames == 0);
    CHECK(detector.estimate()->windowFrames == 60);
}

TEST_CASE("Thirty frames per second reads as half the container rate")
{
    CadenceDetector detector;

    for (int window = 0; window < 2; ++window)
    {
        for (int index = 0; index < 30; ++index)
        {
            detector.observe(distinctFrame());
            detector.observe(repeatedFrame());
        }
    }

    REQUIRE(detector.estimate().has_value());
    CHECK(detector.estimate()->framesPerSecond == Catch::Approx(30.0));
    CHECK(detector.estimate()->distinctFrames == 30);
    CHECK(detector.estimate()->repeatedFrames == 30);
}

TEST_CASE("A 2:3 pulldown reads as 24 frames per second")
{
    CadenceDetector detector;

    for (int window = 0; window < 2; ++window)
    {
        for (int group = 0; group < 12; ++group)
        {
            detector.observe(distinctFrame());
            detector.observe(repeatedFrame());
            detector.observe(repeatedFrame());
            detector.observe(distinctFrame());
            detector.observe(repeatedFrame());
        }
    }

    REQUIRE(detector.estimate().has_value());
    CHECK(detector.estimate()->framesPerSecond == Catch::Approx(24.0));
}

TEST_CASE("Two windows that disagree do not publish")
{
    CadenceDetector detector;

    feed(detector, 60, 0);
    feed(detector, 30, 30);
    CHECK_FALSE(detector.estimate().has_value());

    feed(detector, 30, 30);
    REQUIRE(detector.estimate().has_value());
    CHECK(detector.estimate()->framesPerSecond == Catch::Approx(30.0));
}

TEST_CASE("A static window holds the last estimate")
{
    CadenceDetector detector;

    for (int window = 0; window < 2; ++window)
    {
        feed(detector, 30, 30);
    }

    REQUIRE(detector.estimate().has_value());

    feed(detector, 5, 55);
    REQUIRE(detector.estimate().has_value());
    CHECK(detector.estimate()->framesPerSecond == Catch::Approx(30.0));
    CHECK(detector.windowFrames() == 60);
    CHECK(detector.repeatedFrames() == 55);
}

TEST_CASE("Reset drops the windows and the estimate")
{
    CadenceDetector detector;

    for (int window = 0; window < 2; ++window)
    {
        feed(detector, 30, 30);
    }

    REQUIRE(detector.estimate().has_value());

    detector.reset();
    CHECK_FALSE(detector.estimate().has_value());
    CHECK(detector.windowFrames() == 0);

    feed(detector, 30, 30);
    CHECK_FALSE(detector.estimate().has_value());

    feed(detector, 30, 30);
    REQUIRE(detector.estimate().has_value());
    CHECK(detector.estimate()->framesPerSecond == Catch::Approx(30.0));
}

TEST_CASE("The rolling window keeps the last sixty flags")
{
    CadenceDetector detector;

    feed(detector, 10, 5);
    CHECK(detector.windowFrames() == 15);
    CHECK(detector.repeatedFrames() == 5);

    feed(detector, 0, 60);
    CHECK(detector.windowFrames() == 60);
    CHECK(detector.repeatedFrames() == 60);
}
