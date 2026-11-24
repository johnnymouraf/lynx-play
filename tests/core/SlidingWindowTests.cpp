#include "core/SlidingWindow.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using mfplay::core::SlidingWindow;
using namespace std::chrono_literals;

namespace
{

const SlidingWindow::Clock::time_point kStart{};

} // namespace

TEST_CASE("SlidingWindow reports the rate of the samples in the window")
{
    SlidingWindow window(2s, kStart);

    for (int sample = 0; sample < 60; ++sample)
    {
        window.add(kStart + sample * 16ms);
    }

    const SlidingWindow::Summary summary = window.summarize(kStart + 1000ms);

    CHECK(summary.count == 60U);
    CHECK(summary.perSecond == Catch::Approx(60.0));
}

TEST_CASE("SlidingWindow forgets samples older than its window")
{
    SlidingWindow window(2s, kStart);

    for (int sample = 0; sample < 60; ++sample)
    {
        window.add(kStart + 1000ms + sample * 16ms);
    }

    const SlidingWindow::Summary summary = window.summarize(kStart + 4500ms);

    CHECK(summary.count == 0U);
    CHECK(summary.perSecond == 0.0);
}

TEST_CASE("SlidingWindow follows the moving window, not the whole stream")
{
    SlidingWindow window(2s, kStart);

    for (int sample = 0; sample < 60; ++sample)
    {
        window.add(kStart + sample * 16ms);
    }

    for (int sample = 0; sample < 30; ++sample)
    {
        window.add(kStart + 2000ms + sample * 16ms);
    }

    const SlidingWindow::Summary summary = window.summarize(kStart + 3000ms);

    // Only the second burst is inside the last two seconds, over a two-second
    // window.
    CHECK(summary.count == 30U);
    CHECK(summary.perSecond == Catch::Approx(15.0));
}

TEST_CASE("SlidingWindow averages and keeps the largest value")
{
    SlidingWindow window(2s, kStart);
    window.add(kStart + 100ms, 2.0);
    window.add(kStart + 200ms, 6.0);
    window.add(kStart + 300ms, 4.0);

    const SlidingWindow::Summary summary = window.summarize(kStart + 400ms);

    CHECK(summary.count == 3U);
    CHECK(summary.sum == Catch::Approx(12.0));
    CHECK(summary.average == Catch::Approx(4.0));
    CHECK(summary.max == Catch::Approx(6.0));
}

TEST_CASE("A new SlidingWindow reports its rate over the time it has seen")
{
    SlidingWindow window(2s, kStart);

    for (int sample = 0; sample < 30; ++sample)
    {
        window.add(kStart + sample * 16ms);
    }

    const SlidingWindow::Summary summary = window.summarize(kStart + 500ms);

    CHECK(summary.perSecond == Catch::Approx(60.0));
    CHECK(summary.elapsedSeconds == Catch::Approx(0.5));
}

TEST_CASE("An empty SlidingWindow reports quiet numbers")
{
    SlidingWindow window(2s, kStart);

    const SlidingWindow::Summary summary = window.summarize(kStart + 1000ms);

    CHECK(summary.count == 0U);
    CHECK(summary.perSecond == 0.0);
    CHECK(summary.average == 0.0);
    CHECK(summary.max == 0.0);
}
