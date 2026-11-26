#include "remoteplay/ChiakiLogSink.h"

#include <charconv>
#include <utility>

#include <spdlog/spdlog.h>

namespace mfplay::remoteplay
{

namespace
{

constexpr std::uint32_t kLoggedLevels =
    CHIAKI_LOG_VERBOSE | CHIAKI_LOG_INFO | CHIAKI_LOG_WARNING | CHIAKI_LOG_ERROR;
constexpr std::string_view kConnectionQuality = "connection quality: ";
constexpr std::string_view kTargetBitrateKey = "target_bitrate=";
constexpr auto kQualityInterval = std::chrono::seconds(5);
constexpr auto kRepeatInterval = std::chrono::minutes(1);
constexpr std::size_t kMaxRememberedMessages = 256;
constexpr std::string_view kWithheld = "a message was withheld: it may carry addresses";

spdlog::level::level_enum toSpdlogLevel(ChiakiLogLevel level)
{
    switch (level)
    {
    case CHIAKI_LOG_ERROR:
        return spdlog::level::err;
    case CHIAKI_LOG_WARNING:
        return spdlog::level::warn;
    case CHIAKI_LOG_INFO:
        return spdlog::level::info;
    case CHIAKI_LOG_VERBOSE:
    case CHIAKI_LOG_DEBUG:
        return spdlog::level::debug;
    }

    return spdlog::level::info;
}

// The console's target_bitrate out of a "connection quality" report, or -1.
long targetBitrateOf(std::string_view report)
{
    const auto key = report.find(kTargetBitrateKey);

    if (key == std::string_view::npos)
    {
        return -1;
    }

    const auto digits = report.substr(key + kTargetBitrateKey.size());
    long value = -1;
    std::from_chars(digits.data(), digits.data() + digits.size(), value);

    return value;
}

} // namespace

void discardChiakiLog(ChiakiLogLevel /*level*/, const char* /*message*/, void* /*user*/)
{
}

ChiakiLogSink::ChiakiLogSink(Observer observer) : observer_(std::move(observer))
{
    chiaki_log_init(&log_, kLoggedLevels, &ChiakiLogSink::forward, this);
}

ChiakiLog* ChiakiLogSink::log()
{
    return &log_;
}

void ChiakiLogSink::setRedacted(bool redacted)
{
    redacted_ = redacted;
}

void ChiakiLogSink::forward(ChiakiLogLevel level, const char* message, void* user)
{
    auto& sink = *static_cast<ChiakiLogSink*>(user);
    const std::string text(message);

    if (level == CHIAKI_LOG_VERBOSE)
    {
        sink.logConnectionQuality(text);
        return;
    }

    if (sink.observer_)
    {
        sink.observer_(level, text);
    }

    const bool withheld = sink.redacted_;

    if (withheld && level != CHIAKI_LOG_WARNING && level != CHIAKI_LOG_ERROR)
    {
        return;
    }

    const std::string logged = withheld ? std::string(kWithheld) : text;

    if (sink.isRepeat(logged))
    {
        return;
    }

    spdlog::log(toSpdlogLevel(level), "[chiaki] {}", logged);
}

void ChiakiLogSink::logConnectionQuality(const std::string& message)
{
    const auto start = message.find(kConnectionQuality);

    if (start == std::string::npos)
    {
        return;
    }

    const std::string_view report = std::string_view(message).substr(start);
    const long target = targetBitrateOf(report);
    const auto now = std::chrono::steady_clock::now();

    {
        const std::scoped_lock lock(mutex_);
        const bool changed = target != lastTargetBitrate_;

        if (!changed && now - lastQualityLogged_ < kQualityInterval)
        {
            return;
        }

        lastTargetBitrate_ = target;
        lastQualityLogged_ = now;
    }

    spdlog::info("[chiaki] Console {}", report);
}

bool ChiakiLogSink::isRepeat(const std::string& message)
{
    const auto now = std::chrono::steady_clock::now();
    const std::scoped_lock lock(mutex_);

    if (lastLogged_.size() > kMaxRememberedMessages)
    {
        lastLogged_.clear();
    }

    const auto [entry, inserted] = lastLogged_.try_emplace(message, now);

    if (inserted)
    {
        return false;
    }

    if (now - entry->second < kRepeatInterval)
    {
        return true;
    }

    entry->second = now;
    return false;
}

} // namespace mfplay::remoteplay
