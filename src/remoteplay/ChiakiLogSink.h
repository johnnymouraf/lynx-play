#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <chiaki/log.h>

namespace mfplay::remoteplay
{

// A ChiakiLog callback that drops every message, for the calls whose logs
// could carry tokens or addresses.
void discardChiakiLog(ChiakiLogLevel level, const char* message, void* user);

// A ChiakiLog that sends chiaki-lib's info, warnings and errors to spdlog,
// without its verbose hexdumps, except the console's "connection quality"
// reports: those go out at info, at most every few seconds or when the
// console's target bitrate changes. A message chiaki repeats (a failing ping every
// second) is logged at most once a minute.
class ChiakiLogSink
{
public:
    using Observer = std::function<void(ChiakiLogLevel level, std::string_view message)>;

    // `observer`, when set, also sees every message, on chiaki's threads.
    explicit ChiakiLogSink(Observer observer = {});

    ChiakiLogSink(const ChiakiLogSink&) = delete;
    ChiakiLogSink& operator=(const ChiakiLogSink&) = delete;
    ChiakiLogSink(ChiakiLogSink&&) = delete;
    ChiakiLogSink& operator=(ChiakiLogSink&&) = delete;
    ~ChiakiLogSink() = default;

    [[nodiscard]] ChiakiLog* log();

    // While on, spdlog gets only a notice that a warning or error was
    // withheld, and nothing else: chiaki's messages can carry the console's
    // address, which over the PSN is not on the local network.
    void setRedacted(bool redacted);

private:
    static void forward(ChiakiLogLevel level, const char* message, void* user);
    void logConnectionQuality(const std::string& message);
    [[nodiscard]] bool isRepeat(const std::string& message);

    Observer observer_;
    std::atomic<bool> redacted_ = false;
    ChiakiLog log_{};
    std::mutex mutex_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastLogged_;
    std::chrono::steady_clock::time_point lastQualityLogged_{};
    long lastTargetBitrate_ = -1;
};

} // namespace mfplay::remoteplay
