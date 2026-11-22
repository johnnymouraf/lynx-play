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

    CHECK(snapshot.receivedFps == Catch::Approx(60.0));
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
    metrics.addPresented(kStart + 137ms, std::nullopt, std::nullopt, false);

    const StreamMetricsSnapshot snapshot = atOneSecond(metrics);

    REQUIRE(snapshot.frameTimeCount == 2);
    CHECK(snapshot.frameTimes.at(0) == Catch::Approx(16.0F));
    CHECK(snapshot.frameTimes.at(1) == Catch::Approx(17.0F));
    CHECK(snapshot.presentIntervalValid);
    CHECK(snapshot.presentIntervalMilliseconds == Catch::Approx(16.5));
    CHECK(snapshot.readyToPresentValid);
    CHECK(snapshot.readyToPresentMilliseconds == Catch::Approx(3.0));
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
    CHECK(snapshot.receivedFps == 0.0);
    CHECK(snapshot.bitrateMbps == 0.0);
}
