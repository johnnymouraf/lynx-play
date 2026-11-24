#include "render/GpuTimer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace mfplay::render;

TEST_CASE("The pipeline's stage list keeps both upscalers and the flow")
{
    CHECK(kEffectStageCount == 8);
    CHECK(stageName(EffectStage::VideoSuperResolution) == "VSR");
    CHECK(stageName(EffectStage::DlssSuperResolution) == "DLSS SR");
    CHECK(stageName(EffectStage::OpticalFlow) == "optical flow");

    std::array<std::string_view, kEffectStageCount> names;

    for (std::size_t index = 0; index < kEffectStageCount; ++index)
    {
        names.at(index) = stageName(static_cast<EffectStage>(index));
    }

    std::ranges::sort(names);
    CHECK(std::ranges::adjacent_find(names) == names.end());
    CHECK(std::ranges::find(names, "unknown") == names.end());
}

TEST_CASE("GPU timestamps become per-stage and total milliseconds")
{
    // One tick per microsecond.
    constexpr std::uint64_t frequency = 1'000'000;
    FrameTimestamps timestamps;
    timestamps.ticks = {1'000,
                        1'100, 1'350,
                        1'350, 1'500,
                        1'500, 2'000,
                        2'000, 2'100,
                        2'100, 2'250,
                        2'250, 2'350,
                        2'350, 2'700,
                        2'700, 2'800,
                        2'800};

    const auto times = frameGpuTimes(timestamps, frequency);

    REQUIRE(times.has_value());
    CHECK(times->stages.at(0) == Catch::Approx(0.25));
    CHECK(times->stages.at(1) == Catch::Approx(0.15));
    CHECK(times->stages.at(2) == Catch::Approx(0.50));
    CHECK(times->stages.at(3) == Catch::Approx(0.10));
    CHECK(times->stages.at(4) == Catch::Approx(0.15));
    CHECK(times->stages.at(5) == Catch::Approx(0.10));
    CHECK(times->stages.at(6) == Catch::Approx(0.35));
    CHECK(times->stages.at(7) == Catch::Approx(0.10));
    CHECK(times->total == Catch::Approx(1.80));
}

TEST_CASE("A frame without stamps has no GPU times")
{
    const FrameTimestamps timestamps;

    CHECK_FALSE(frameGpuTimes(timestamps, 1'000'000).has_value());
    CHECK_FALSE(frameGpuTimes(timestamps, 0).has_value());
}

TEST_CASE("A stage that ends before it starts invalidates the frame")
{
    FrameTimestamps timestamps;
    timestamps.ticks = {100,
                        200, 190,
                        200, 300,
                        300, 400,
                        400, 500,
                        500, 600,
                        600, 700,
                        700, 800,
                        800, 900,
                        900};

    CHECK_FALSE(frameGpuTimes(timestamps, 1'000'000).has_value());
}
