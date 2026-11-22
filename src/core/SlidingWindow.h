#pragma once

#include <chrono>
#include <cstdint>
#include <deque>

namespace lynx::core
{

// Counts events and sums the values they carry over a moving time window:
// rates, averages and maxima follow the last minutes of a stream instead of
// one reporting period.
class SlidingWindow
{
public:
    using Clock = std::chrono::steady_clock;

    struct Summary
    {
        std::uint64_t count = 0;
        double sum = 0.0;
        double perSecond = 0.0;
        double average = 0.0;
        double max = 0.0;
        // The time the rate was measured over; at most the window's length.
        double elapsedSeconds = 0.0;
    };

    SlidingWindow(Clock::duration window, Clock::time_point start);

    void add(Clock::time_point at, double value = 0.0);

    // Drops what left the window and sums what remains.
    [[nodiscard]] Summary summarize(Clock::time_point now);

private:
    struct Sample
    {
        Clock::time_point at;
        double value = 0.0;
    };

    void dropExpired(Clock::time_point now);

    Clock::duration window_;
    Clock::time_point start_;
    std::deque<Sample> samples_;
};

} // namespace lynx::core
