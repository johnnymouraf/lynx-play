#include "remoteplay/ChiakiLogSink.h"

#include <utility>

#include <spdlog/spdlog.h>

namespace mfplay::remoteplay
{

namespace
{

constexpr std::uint32_t kLoggedLevels = CHIAKI_LOG_INFO | CHIAKI_LOG_WARNING | CHIAKI_LOG_ERROR;
constexpr auto kRepeatInterval = std::chrono::minutes(1);
constexpr std::size_t kMaxRememberedMessages = 256;

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

} // namespace

ChiakiLogSink::ChiakiLogSink(Observer observer) : observer_(std::move(observer))
{
    chiaki_log_init(&log_, kLoggedLevels, &ChiakiLogSink::forward, this);
}

ChiakiLog* ChiakiLogSink::log()
{
    return &log_;
}

void ChiakiLogSink::forward(ChiakiLogLevel level, const char* message, void* user)
{
    auto& sink = *static_cast<ChiakiLogSink*>(user);
    const std::string text(message);

    if (sink.observer_)
    {
        sink.observer_(level, text);
    }

    if (sink.isRepeat(text))
    {
        return;
    }

    spdlog::log(toSpdlogLevel(level), "[chiaki] {}", text);
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
