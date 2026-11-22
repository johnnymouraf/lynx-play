#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace lynx::core
{

// What happened during one reporting period.
struct RateSummary
{
    std::uint64_t count = 0;
    double perSecond = 0.0;
    // Of the durations added with each event; zero when none were.
    double averageMilliseconds = 0.0;
    double maxMilliseconds = 0.0;
};

// Counts events (decoded or presented frames) and how long each took, and
// sums them up once per reporting period.
class RateCounter
{
public:
    using Clock = std::chrono::steady_clock;

    RateCounter(Clock::time_point start, Clock::duration period);

    void add(Clock::duration duration = Clock::duration::zero());

    // The summary of the period that ended, once `now` is past its end; the
    // next period starts at `now`.
    [[nodiscard]] std::optional<RateSummary> takeIfDue(Clock::time_point now);

private:
    Clock::time_point periodStart_;
    Clock::duration period_;
    std::uint64_t count_ = 0;
    Clock::duration total_ = Clock::duration::zero();
    Clock::duration longest_ = Clock::duration::zero();
};

} // namespace lynx::core
