#pragma once

#include "core/Windows.h"
#include "render/VideoFrame.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace mfplay::render
{

// Hands frames from a video source's thread to the renderer. Only the newest
// frame waits: one the renderer hasn't taken when the next arrives is dropped,
// so the picture never falls behind the source.
class FrameMailbox
{
public:
    [[nodiscard]] static std::expected<std::shared_ptr<FrameMailbox>, std::string> create();

    FrameMailbox(const FrameMailbox&) = delete;
    FrameMailbox& operator=(const FrameMailbox&) = delete;
    FrameMailbox(FrameMailbox&&) = delete;
    FrameMailbox& operator=(FrameMailbox&&) = delete;
    ~FrameMailbox() = default;

    void publish(VideoFrame frame);

    // The newest frame published since the last call, if any.
    [[nodiscard]] std::optional<VideoFrame> take();

    // Signaled by publish; a wait that wakes on it resets it.
    [[nodiscard]] HANDLE frameReadyEvent() const;

    // How many frames were replaced before being taken since the last call.
    [[nodiscard]] std::uint64_t takeDroppedCount();

private:
    explicit FrameMailbox(core::UniqueHandle frameReady);

    std::mutex mutex_;
    std::optional<VideoFrame> pending_;
    std::uint64_t dropped_ = 0;
    core::UniqueHandle frameReady_;
};

} // namespace mfplay::render
