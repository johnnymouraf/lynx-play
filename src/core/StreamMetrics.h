#pragma once

#include "core/SlidingWindow.h"
#include "core/VideoCodec.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace mfplay::core
{

inline constexpr std::size_t kFrameTimeHistory = 120;

// The numbers the overlay shows about the video path, all over the last two
// seconds (the frame times keep a longer history for the plot).
struct StreamMetricsSnapshot
{
    bool formatValid = false;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::string codec;

    // The container rate: access units the decoder received per second.
    double sourceFps = 0.0;
    // Distinct source pictures per second: the game's own rate.
    double gameFps = 0.0;
    // Pictures DLSS Frame Generation produced and presented per second.
    double generatedFps = 0.0;
    // New pictures shown per second, the distinct source ones plus the
    // generated ones. With Frame Generation off it equals the game rate.
    double displayedFps = 0.0;
    // Real pictures shown per second and, of those, the ones that repeated
    // a picture already on screen. Frame Generation holds the real picture
    // for the repeat, so it presents no repeated real picture.
    double realPresentedFps = 0.0;
    double repeatedPresentedFps = 0.0;
    double bitrateMbps = 0.0;

    bool packetLossValid = false;
    double packetLossPercent = 0.0;

    bool decodeValid = false;
    double decodeMilliseconds = 0.0;
    double decodeMaxMilliseconds = 0.0;
    bool presentIntervalValid = false;
    double presentIntervalMilliseconds = 0.0;
    bool readyToPresentValid = false;
    double readyToPresentMilliseconds = 0.0;

    std::array<float, kFrameTimeHistory> frameTimes{};
    std::size_t frameTimeCount = 0;
};

// The counters the decoder, the session and the renderer feed, safe to read
// from any of their threads.
class StreamMetrics
{
public:
    using Clock = std::chrono::steady_clock;

    explicit StreamMetrics(Clock::time_point start = Clock::now());

    void setCodec(VideoCodec codec);
    void setFormat(std::uint32_t width, std::uint32_t height);

    // One access unit as it reaches the decoder.
    void addReceived(Clock::time_point at, std::uint64_t bytes);
    // One picture the decoder finished, with how long it took.
    void addDecoded(Clock::time_point at, double milliseconds);
    // One source picture the cadence detector classified: a distinct one is
    // a new game picture and counts as shown.
    void addCadenceFrame(Clock::time_point at, bool distinct);
    // Packet counters since the previous call, from the session.
    void addPacketStats(Clock::time_point at, std::uint64_t received, std::uint64_t lost);
    // One real present; `newFrame` says it showed a picture the decoder just
    // produced, not a redraw of the one already on screen. The interval is
    // absent for the first new frame, and so is the time from frame ready.
    void addPresented(Clock::time_point at, std::optional<Clock::duration> interval,
                       std::optional<Clock::duration> readyToPresent, bool newFrame);
    // One present of a picture DLSS Frame Generation produced.
    void addGeneratedPresent(Clock::time_point at);

    [[nodiscard]] StreamMetricsSnapshot snapshot(Clock::time_point now);

private:
    std::mutex mutex_;
    bool formatValid_ = false;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::string codec_;
    SlidingWindow received_;
    SlidingWindow distinct_;
    SlidingWindow generated_;
    SlidingWindow presented_;
    SlidingWindow decodeTime_;
    SlidingWindow presentInterval_;
    SlidingWindow readyToPresent_;
    SlidingWindow receivedBytes_;
    SlidingWindow packetsReceived_;
    SlidingWindow packetsLost_;
    std::array<float, kFrameTimeHistory> frameTimes_{};
    std::size_t frameTimeCount_ = 0;
    std::size_t frameTimeNext_ = 0;
};

} // namespace mfplay::core
