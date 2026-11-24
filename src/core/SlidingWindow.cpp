#include "core/SlidingWindow.h"

#include <algorithm>
#include <utility>

namespace mfplay::core
{

SlidingWindow::SlidingWindow(Clock::duration window, Clock::time_point start)
    : window_(window), start_(start)
{
}

void SlidingWindow::add(Clock::time_point at, double value)
{
    dropExpired(at);
    samples_.push_back({.at = at, .value = value});
}

SlidingWindow::Summary SlidingWindow::summarize(Clock::time_point now)
{
    dropExpired(now);

    Summary summary;

    for (const Sample& sample : samples_)
    {
        ++summary.count;
        summary.sum += sample.value;
        summary.max = std::max(summary.max, sample.value);
    }

    const Clock::time_point windowStart = now - window_;
    const Clock::time_point effectiveStart = std::max(windowStart, start_);
    const Clock::duration elapsed = now - effectiveStart;

    if (elapsed > Clock::duration::zero())
    {
        summary.elapsedSeconds = std::chrono::duration<double>(elapsed).count();
        summary.perSecond = static_cast<double>(summary.count) / summary.elapsedSeconds;
    }

    if (summary.count > 0)
    {
        summary.average = summary.sum / static_cast<double>(summary.count);
    }

    return summary;
}

void SlidingWindow::dropExpired(Clock::time_point now)
{
    const Clock::time_point cutoff = now - window_;

    while (!samples_.empty() && samples_.front().at <= cutoff)
    {
        samples_.pop_front();
    }
}

} // namespace mfplay::core
