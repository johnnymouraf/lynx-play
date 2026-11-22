#pragma once

#include "core/VideoCodec.h"
#include "render/FrameMailbox.h"
#include "render/GpuDevice.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>

namespace lynx::media
{

// Decodes H.264 or H.265 on the GPU with FFmpeg's D3D12VA, straight into
// textures on the renderer's device, and publishes each picture to a mailbox.
// It never falls back to decoding on the CPU. One thread feeds it.
class VideoDecoder
{
public:
    [[nodiscard]] static std::expected<std::unique_ptr<VideoDecoder>, std::string> create(
        const render::GpuDevice& gpu, core::VideoCodec codec,
        std::shared_ptr<render::FrameMailbox> mailbox);

    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;
    VideoDecoder(VideoDecoder&&) = delete;
    VideoDecoder& operator=(VideoDecoder&&) = delete;
    ~VideoDecoder();

    // Decodes one access unit (a whole picture, Annex B) and publishes the
    // picture it completes once the GPU has finished it. An error means corrupt
    // data or a failing decoder; decoding recovers at the next keyframe.
    [[nodiscard]] std::expected<void, std::string> decode(
        std::span<const std::uint8_t> accessUnit);

private:
    struct Context;

    explicit VideoDecoder(std::unique_ptr<Context> context);

    std::unique_ptr<Context> context_;
};

} // namespace lynx::media
