#include "core/StreamMetrics.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using lynx::core::StreamMetrics;
using lynx::core::StreamMetricsSnapshot;
using namespace std::chrono_literals;

namespace
{

const StreamMetrics::Clock::time_point kStart{};

StreamMetricsSnapshot atOneSecond(StreamMetrics& metrics)
{
    return metrics.snapshot(kStart + 1000ms);
}

} // namespace

TEST_CASE("StreamMetrics turns received bytes into the stream's bitrate")
{
    StreamMetrics metrics(kStart);

    // 60 access units of 100 kB in one second: 48 Mbps.
    for (int frame = 0; frame < 60; ++frame)
    {
        metrics.addReceived(kStart + (frame + 1) * 16ms, 100'000);
    }

    const StreamMetricsSnapshot snapshot = atOneSecond(metrics);

    CHECK(snapshot.sourceFps == Catch::Approx(60.0));
    CHECK(snapshot.bitrateMbps == Catch::Approx(48.0).margin(0.1));
}

TEST_CASE("StreamMetrics reports the packet loss of the window")
{
    StreamMetrics metrics(kStart);
    metrics.addPacketStats(kStart + 100ms, 90, 10);
    metrics.addPacketStats(kStart + 200ms, 180, 20);

    const StreamMetricsSnapshot snapshot = atOneSecond(metrics);

    CHECK(snapshot.packetLossValid);
    CHECK(snapshot.packetLossPercent == Catch::Approx(10.0));
}

TEST_CASE("StreamMetrics averages decode times and keeps the longest")
{
    StreamMetrics metrics(kStart);
    metrics.addDecoded(kStart + 100ms, 2.0);
    metrics.addDecoded(kStart + 200ms, 4.0);

    const StreamMetricsSnapshot snapshot = atOneSecond(metrics);

    CHECK(snapshot.decodeValid);
    CHECK(snapshot.decodeMilliseconds == Catch::Approx(3.0));
    CHECK(snapshot.decodeMaxMilliseconds == Catch::Approx(4.0));
}

TEST_CASE("StreamMetrics keeps present intervals for the frame time plot")
{
    StreamMetrics metrics(kStart);

    metrics.addPresented(kStart + 100ms, std::nullopt, 2ms, true);
    metrics.addPresented(kStart + 116ms, 16ms, 3ms, true);
    metrics.addPresented(kStart + 133ms, 17ms, 4ms, true);
    metrics.addPresented(kStart + 150ms, std::nullopt, std::nullopt, false);

    const StreamMetricsSnapshot snapshot = atOneSecond(metrics);

    REQUIRE(snapshot.frameTimeCount == 2);
    CHECK(snapshot.frameTimes.at(0) == Catch::Approx(16.0F));
    CHECK(snapshot.frameTimes.at(1) == Catch::Approx(17.0F));
    CHECK(snapshot.presentIntervalValid);
    CHECK(snapshot.presentIntervalMilliseconds == Catch::Approx(16.5));
    CHECK(snapshot.readyToPresentValid);
    CHECK(snapshot.readyToPresentMilliseconds == Catch::Approx(3.0));
    // The redraw at 150 ms presented the picture already on screen.
    CHECK(snapshot.realPresentedFps == Catch::Approx(3.0));
}

TEST_CASE("StreamMetrics separates the source and the game rate of a 30 in 60 stream")
{
    StreamMetrics metrics(kStart);

    // Sixty access units arrive per second, but only every other picture is
    // distinct: the game runs at 30. Without generation, every picture is
    // shown, repeats included.
    for (int frame = 0; frame < 60; ++frame)
    {
        const auto at = kStart + (frame + 1) * 16ms;
        metrics.addReceived(at, 100'000);
        metrics.addCadenceFrame(at, frame % 2 == 0);
        metrics.addPresented(at, std::nullopt, std::nullopt, true);
    }

    const StreamMetricsSnapshot snapshot = atOneSecond(metrics);

    CHECK(snapshot.sourceFps == Catch::Approx(60.0));
    CHECK(snapshot.gameFps == Catch::Approx(30.0));
    CHECK(snapshot.generatedFps == 0.0);
    CHECK(snapshot.displayedFps == Catch::Approx(30.0));
    CHECK(snapshot.realPresentedFps == Catch::Approx(60.0));
    CHECK(snapshot.repeatedPresentedFps == Catch::Approx(30.0));
}

TEST_CASE("StreamMetrics counts generated pictures in the displayed rate")
{
    StreamMetrics metrics(kStart);

    // The same 30 fps game with Frame Generation 2x: one generated picture
    // per distinct source picture, and the real one held for the repeat, so
    // the two presents per pair carry one picture each.
    for (int frame = 0; frame < 60; ++frame)
    {
        const auto at = kStart + (frame + 1) * 16ms;
        const bool distinct = frame % 2 == 0;
        metrics.addReceived(at, 100'000);
        metrics.addCadenceFrame(at, distinct);

        if (distinct)
        {
            metrics.addGeneratedPresent(at);
            continue;
        }

        metrics.addPresented(at, std::nullopt, std::nullopt, true);
    }

    const StreamMetricsSnapshot snapshot = atOneSecond(metrics);

    CHECK(snapshot.sourceFps == Catch::Approx(60.0));
    CHECK(snapshot.gameFps == Catch::Approx(30.0));
    CHECK(snapshot.generatedFps == Catch::Approx(30.0));
    CHECK(snapshot.displayedFps == Catch::Approx(60.0));
    CHECK(snapshot.realPresentedFps == Catch::Approx(30.0));
    CHECK(snapshot.repeatedPresentedFps == 0.0);
}

TEST_CASE("StreamMetrics doubles a true 60 fps game with Frame Generation")
{
    StreamMetrics metrics(kStart);

    for (int frame = 0; frame < 60; ++frame)
    {
        const auto at = kStart + (frame + 1) * 16ms;
        metrics.addReceived(at, 100'000);
        metrics.addCadenceFrame(at, true);
        metrics.addGeneratedPresent(at);
        metrics.addPresented(at, std::nullopt, std::nullopt, true);
    }

    const StreamMetricsSnapshot snapshot = atOneSecond(metrics);

    CHECK(snapshot.sourceFps == Catch::Approx(60.0));
    CHECK(snapshot.gameFps == Catch::Approx(60.0));
    CHECK(snapshot.generatedFps == Catch::Approx(60.0));
    CHECK(snapshot.displayedFps == Catch::Approx(120.0));
    CHECK(snapshot.realPresentedFps == Catch::Approx(60.0));
    CHECK(snapshot.repeatedPresentedFps == 0.0);
}

TEST_CASE("StreamMetrics drops the rates of frames outside the window")
{
    StreamMetrics metrics(kStart);

    for (int frame = 0; frame < 60; ++frame)
    {
        const auto at = kStart + (frame + 1) * 16ms;
        metrics.addReceived(at, 100'000);
        metrics.addCadenceFrame(at, true);
        metrics.addGeneratedPresent(at);
        metrics.addPresented(at, std::nullopt, std::nullopt, true);
    }

    const StreamMetricsSnapshot snapshot = metrics.snapshot(kStart + 3000ms);

    CHECK(snapshot.sourceFps == 0.0);
    CHECK(snapshot.gameFps == 0.0);
    CHECK(snapshot.generatedFps == 0.0);
    CHECK(snapshot.displayedFps == 0.0);
    CHECK(snapshot.realPresentedFps == 0.0);
    CHECK(snapshot.repeatedPresentedFps == 0.0);
}

TEST_CASE("StreamMetrics carries the codec and the decoded size")
{
    StreamMetrics metrics(kStart);
    metrics.setCodec(lynx::core::VideoCodec::H265);
    metrics.setFormat(1920, 1080);

    const StreamMetricsSnapshot snapshot = atOneSecond(metrics);

    CHECK(snapshot.formatValid);
    CHECK(snapshot.width == 1920U);
    CHECK(snapshot.height == 1080U);
    CHECK(snapshot.codec == "H.265");
}

TEST_CASE("StreamMetrics reports quiet counters before the stream starts")
{
    StreamMetrics metrics(kStart);

    const StreamMetricsSnapshot snapshot = atOneSecond(metrics);

    CHECK_FALSE(snapshot.formatValid);
    CHECK_FALSE(snapshot.packetLossValid);
    CHECK_FALSE(snapshot.decodeValid);
    CHECK_FALSE(snapshot.presentIntervalValid);
    CHECK(snapshot.sourceFps == 0.0);
    CHECK(snapshot.gameFps == 0.0);
    CHECK(snapshot.generatedFps == 0.0);
    CHECK(snapshot.displayedFps == 0.0);
    CHECK(snapshot.realPresentedFps == 0.0);
    CHECK(snapshot.repeatedPresentedFps == 0.0);
    CHECK(snapshot.bitrateMbps == 0.0);
}
