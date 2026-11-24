#include "render/FrameMailbox.h"

#include <utility>

#include <spdlog/spdlog.h>

namespace mfplay::render
{

std::expected<std::shared_ptr<FrameMailbox>, std::string> FrameMailbox::create()
{
    core::UniqueHandle frameReady(CreateEventW(nullptr, FALSE, FALSE, nullptr));

    if (!frameReady)
    {
        return std::unexpected(
            core::describeFailure("CreateEventW", HRESULT_FROM_WIN32(GetLastError())));
    }

    spdlog::info("Frame queue depth 1: only the newest decoded frame waits");

    // The constructor is private, out of make_shared's reach.
    return std::shared_ptr<FrameMailbox>(new FrameMailbox(std::move(frameReady)));
}

FrameMailbox::FrameMailbox(core::UniqueHandle frameReady) : frameReady_(std::move(frameReady))
{
}

void FrameMailbox::publish(VideoFrame frame)
{
    // Released outside the lock: freeing a decoder surface takes the
    // decoder's own locks.
    std::optional<VideoFrame> replaced;

    {
        const std::scoped_lock lock(mutex_);
        replaced = std::exchange(pending_, std::move(frame));

        if (replaced)
        {
            ++dropped_;
        }
    }

    SetEvent(frameReady_.get());
}

std::optional<VideoFrame> FrameMailbox::take()
{
    const std::scoped_lock lock(mutex_);
    return std::exchange(pending_, std::nullopt);
}

HANDLE FrameMailbox::frameReadyEvent() const
{
    return frameReady_.get();
}

std::uint64_t FrameMailbox::takeDroppedCount()
{
    const std::scoped_lock lock(mutex_);
    return std::exchange(dropped_, 0);
}

} // namespace mfplay::render
