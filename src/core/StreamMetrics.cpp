#include "core/StreamMetrics.h"

#include <algorithm>
#include <utility>

namespace mfplay::core
{

namespace
{

constexpr auto kMetricsWindow = std::chrono::seconds(2);
constexpr double kBitsPerMegabit = 1'000'000.0;

double toMilliseconds(SlidingWindow::Clock::duration duration)
{
    return std::chrono::duration<double, std::milli>(duration).count();
}

} // namespace

StreamMetrics::StreamMetrics(Clock::time_point start)
    : received_(kMetricsWindow, start),
      distinct_(kMetricsWindow, start),
      generated_(kMetricsWindow, start),
      presented_(kMetricsWindow, start),
      decodeTime_(kMetricsWindow, start),
      presentInterval_(kMetricsWindow, start),
      readyToPresent_(kMetricsWindow, start),
      receivedBytes_(kMetricsWindow, start),
      packetsReceived_(kMetricsWindow, start),
      packetsLost_(kMetricsWindow, start)
{
}

void StreamMetrics::setCodec(VideoCodec codec)
{
    const std::scoped_lock lock(mutex_);
    codec_ = std::string(codecName(codec));
}

void StreamMetrics::setFormat(std::uint32_t width, std::uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    const std::scoped_lock lock(mutex_);
    formatValid_ = true;
    width_ = width;
    height_ = height;
}

void StreamMetrics::addReceived(Clock::time_point at, std::uint64_t bytes)
{
    const std::scoped_lock lock(mutex_);
    received_.add(at);
    receivedBytes_.add(at, static_cast<double>(bytes));
}

void StreamMetrics::addGeneratedPresent(Clock::time_point at)
{
    const std::scoped_lock lock(mutex_);
    generated_.add(at);
}

void StreamMetrics::addDecoded(Clock::time_point at, double milliseconds)
{
    const std::scoped_lock lock(mutex_);
    decodeTime_.add(at, milliseconds);
}

void StreamMetrics::addCadenceFrame(Clock::time_point at, bool distinct)
{
    if (!distinct)
    {
        return;
    }

    const std::scoped_lock lock(mutex_);
    distinct_.add(at);
}

void StreamMetrics::addUnclassifiedFrame(Clock::time_point at)
{
    const std::scoped_lock lock(mutex_);
    unclassified_.add(at);
}

void StreamMetrics::addPacketStats(Clock::time_point at, std::uint64_t received,
                                   std::uint64_t lost)
{
    const std::scoped_lock lock(mutex_);
    packetsReceived_.add(at, static_cast<double>(received));
    packetsLost_.add(at, static_cast<double>(lost));
}

void StreamMetrics::addPresented(Clock::time_point at, std::optional<Clock::duration> interval,
                                 std::optional<Clock::duration> readyToPresent, bool newFrame)
{
    const std::scoped_lock lock(mutex_);

    if (newFrame)
    {
        presented_.add(at);
    }

    if (interval)
    {
        presentInterval_.add(at, toMilliseconds(*interval));

        frameTimes_.at(frameTimeNext_) = static_cast<float>(toMilliseconds(*interval));
        frameTimeNext_ = (frameTimeNext_ + 1) % kFrameTimeHistory;
        frameTimeCount_ = std::min(frameTimeCount_ + 1, kFrameTimeHistory);
    }

    if (readyToPresent)
    {
        readyToPresent_.add(at, toMilliseconds(*readyToPresent));
    }
}

StreamMetricsSnapshot StreamMetrics::snapshot(Clock::time_point now)
{
    const std::scoped_lock lock(mutex_);

    const SlidingWindow::Summary received = received_.summarize(now);
    const SlidingWindow::Summary distinct = distinct_.summarize(now);
    const SlidingWindow::Summary unclassified = unclassified_.summarize(now);
    const SlidingWindow::Summary generated = generated_.summarize(now);
    const SlidingWindow::Summary presented = presented_.summarize(now);
    const SlidingWindow::Summary decode = decodeTime_.summarize(now);
    const SlidingWindow::Summary interval = presentInterval_.summarize(now);
    const SlidingWindow::Summary ready = readyToPresent_.summarize(now);
    const SlidingWindow::Summary bytes = receivedBytes_.summarize(now);
    const SlidingWindow::Summary packetsIn = packetsReceived_.summarize(now);
    const SlidingWindow::Summary packetsOut = packetsLost_.summarize(now);

    StreamMetricsSnapshot snapshot;
    snapshot.formatValid = formatValid_;
    snapshot.width = width_;
    snapshot.height = height_;
    snapshot.codec = codec_;
    snapshot.sourceFps = received.perSecond;
    snapshot.gameFps = distinct.perSecond;
    snapshot.unclassifiedFps = unclassified.perSecond;
    snapshot.generatedFps = generated.perSecond;
    snapshot.displayedFps = snapshot.gameFps + snapshot.generatedFps;
    snapshot.realPresentedFps = presented.perSecond;
    snapshot.repeatedPresentedFps = std::max(
        0.0, snapshot.realPresentedFps - snapshot.gameFps - snapshot.unclassifiedFps);

    if (bytes.elapsedSeconds > 0.0)
    {
        snapshot.bitrateMbps = bytes.sum * 8.0 / kBitsPerMegabit / bytes.elapsedSeconds;
    }

    const double packetTotal = packetsIn.sum + packetsOut.sum;
    snapshot.packetLossValid = packetsIn.count > 0 || packetsOut.count > 0;
    snapshot.packetLossPercent = packetTotal > 0.0 ? 100.0 * packetsOut.sum / packetTotal : 0.0;

    snapshot.decodeValid = decode.count > 0;
    snapshot.decodeMilliseconds = decode.average;
    snapshot.decodeMaxMilliseconds = decode.max;
    snapshot.presentIntervalValid = interval.count > 0;
    snapshot.presentIntervalMilliseconds = interval.average;
    snapshot.readyToPresentValid = ready.count > 0;
    snapshot.readyToPresentMilliseconds = ready.average;

    snapshot.frameTimeCount = frameTimeCount_;

    const std::size_t first = frameTimeCount_ < kFrameTimeHistory ? 0 : frameTimeNext_;

    for (std::size_t index = 0; index < frameTimeCount_; ++index)
    {
        const std::size_t source = (first + index) % kFrameTimeHistory;
        snapshot.frameTimes.at(index) = frameTimes_.at(source);
    }

    return snapshot;
}

} // namespace mfplay::core
