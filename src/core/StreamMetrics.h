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

namespace lynx::core
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

    double receivedFps = 0.0;
    double decodedFps = 0.0;
    // New decoded pictures shown on screen.
    double presentedFps = 0.0;
    // Every picture shown, real and generated.
    double allPresentedFps = 0.0;
    // Pictures DLSS Frame Generation produced since the stream started.
    std::uint64_t generatedFrames = 0;
    double generatedFps = 0.0;
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
    // Packet counters since the previous call, from the session.
    void addPacketStats(Clock::time_point at, std::uint64_t received, std::uint64_t lost);
    // One present; `uniqueFrame` says it showed a new decoded picture, so it
    // counts as a presented frame. The interval is absent for the first new
    // frame, and the time from the frame being ready only for new frames.
    void addPresented(Clock::time_point at, std::optional<Clock::duration> interval,
                       std::optional<Clock::duration> readyToPresent, bool uniqueFrame);
    // One present of a picture DLSS Frame Generation produced. It counts in
    // the all-presents rate and in the generated count, never as a real
    // presented frame.
    void addGeneratedPresent(Clock::time_point at);

    [[nodiscard]] StreamMetricsSnapshot snapshot(Clock::time_point now);

private:
    std::mutex mutex_;
    bool formatValid_ = false;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::string codec_;
    SlidingWindow received_;
    SlidingWindow decoded_;
    SlidingWindow presented_;
    SlidingWindow allPresented_;
    SlidingWindow generated_;
    std::uint64_t generatedFrames_ = 0;
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

} // namespace lynx::core
