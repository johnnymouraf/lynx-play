#include "core/RateCounter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using lynx::core::RateCounter;
using namespace std::chrono_literals;

namespace
{

const RateCounter::Clock::time_point kStart{};

} // namespace

TEST_CASE("RateCounter reports nothing before its period ends")
{
    RateCounter counter(kStart, 5s);
    counter.add(2ms);

    CHECK_FALSE(counter.takeIfDue(kStart + 4s).has_value());
}

TEST_CASE("RateCounter sums up the period's rate and durations")
{
    RateCounter counter(kStart, 5s);

    for (int frame = 0; frame < 300; ++frame)
    {
        counter.add(frame == 0 ? 6ms : 2ms);
    }

    const auto summary = counter.takeIfDue(kStart + 5s);

    REQUIRE(summary.has_value());
    CHECK(summary->count == 300U);
    CHECK(summary->perSecond == Catch::Approx(60.0));
    CHECK(summary->averageMilliseconds == Catch::Approx((6.0 + 299 * 2.0) / 300));
    CHECK(summary->maxMilliseconds == Catch::Approx(6.0));
}

TEST_CASE("RateCounter starts a new period after each summary")
{
    RateCounter counter(kStart, 5s);
    counter.add(8ms);
    REQUIRE(counter.takeIfDue(kStart + 5s).has_value());

    counter.add(1ms);
    const auto summary = counter.takeIfDue(kStart + 10s);

    REQUIRE(summary.has_value());
    CHECK(summary->count == 1U);
    CHECK(summary->maxMilliseconds == Catch::Approx(1.0));
}

TEST_CASE("RateCounter reports a quiet period as zero")
{
    RateCounter counter(kStart, 5s);

    const auto summary = counter.takeIfDue(kStart + 5s);

    REQUIRE(summary.has_value());
    CHECK(summary->count == 0U);
    CHECK(summary->perSecond == 0.0);
    CHECK(summary->averageMilliseconds == 0.0);
}
