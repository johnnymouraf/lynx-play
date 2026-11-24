#pragma once

#include "core/Windows.h"

#include <chrono>
#include <expected>
#include <stop_token>
#include <string>

namespace mfplay::core
{

// Wakes a thread at a steady frame rate, to within a fraction of a
// millisecond: the standard library's sleeps round up to the 15.6 ms system
// tick, which turns 60 fps into an uneven 64/32 fps cadence.
class FramePacer
{
public:
    using Clock = std::chrono::steady_clock;

    [[nodiscard]] static std::expected<FramePacer, std::string> create(double framesPerSecond);

    // Waits until one period after the previous tick. A thread that fell more
    // than a period behind starts over from now instead of catching up in a
    // burst. False when `stop` was requested.
    [[nodiscard]] bool waitForNextTick(const std::stop_token& stop);

private:
    FramePacer(UniqueHandle timer, UniqueHandle stopEvent, Clock::duration period);

    UniqueHandle timer_;
    UniqueHandle stopEvent_;
    Clock::duration period_;
    Clock::time_point nextTick_;
};

} // namespace mfplay::core
