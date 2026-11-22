#include "core/RateCounter.h"

#include <algorithm>

namespace lynx::core
{

namespace
{

double toMilliseconds(RateCounter::Clock::duration duration)
{
    return std::chrono::duration<double, std::milli>(duration).count();
}

} // namespace

RateCounter::RateCounter(Clock::time_point start, Clock::duration period)
    : periodStart_(start), period_(period)
{
}

void RateCounter::add(Clock::duration duration)
{
    ++count_;
    total_ += duration;
    longest_ = std::max(longest_, duration);
}

std::optional<RateSummary> RateCounter::takeIfDue(Clock::time_point now)
{
    const Clock::duration elapsed = now - periodStart_;

    if (elapsed < period_)
    {
        return std::nullopt;
    }

    const double seconds = std::chrono::duration<double>(elapsed).count();
    RateSummary summary{
        .count = count_,
        .perSecond = static_cast<double>(count_) / seconds,
        .averageMilliseconds = 0.0,
        .maxMilliseconds = toMilliseconds(longest_),
    };

    if (count_ > 0)
    {
        summary.averageMilliseconds = toMilliseconds(total_) / static_cast<double>(count_);
    }

    periodStart_ = now;
    count_ = 0;
    total_ = Clock::duration::zero();
    longest_ = Clock::duration::zero();

    return summary;
}

} // namespace lynx::core
