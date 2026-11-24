#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <chiaki/log.h>

namespace mfplay::remoteplay
{

// A ChiakiLog that sends chiaki-lib's info, warnings and errors to spdlog,
// without its verbose hexdumps. A message chiaki repeats (a failing ping every
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

private:
    static void forward(ChiakiLogLevel level, const char* message, void* user);
    [[nodiscard]] bool isRepeat(const std::string& message);

    Observer observer_;
    ChiakiLog log_{};
    std::mutex mutex_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> lastLogged_;
};

} // namespace mfplay::remoteplay
