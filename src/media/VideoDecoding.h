#pragma once

#include "core/VideoCodec.h"

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>

namespace mfplay::media
{

// Decodes the access units of one stream into pictures. Each platform provides
// its own decoder: the Windows build decodes on the GPU (VideoDecoder).
class VideoDecoding
{
public:
    VideoDecoding() = default;
    VideoDecoding(const VideoDecoding&) = delete;
    VideoDecoding& operator=(const VideoDecoding&) = delete;
    VideoDecoding(VideoDecoding&&) = delete;
    VideoDecoding& operator=(VideoDecoding&&) = delete;
    virtual ~VideoDecoding() = default;

    // Decodes one access unit (a whole picture, Annex B). An error means corrupt
    // data or a failing decoder; decoding recovers at the next keyframe.
    [[nodiscard]] virtual std::expected<void, std::string> decode(
        std::span<const std::uint8_t> accessUnit) = 0;
};

using VideoDecoderResult = std::expected<std::unique_ptr<VideoDecoding>, std::string>;

// Opens the decoder of a session for its codec. `hdrExpected` is the session's
// HDR request, as VideoDecoder::create takes it.
using VideoDecoderFactory =
    std::function<VideoDecoderResult(core::VideoCodec codec, bool hdrExpected)>;

} // namespace mfplay::media
