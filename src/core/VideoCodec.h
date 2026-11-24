#pragma once

#include <string_view>

namespace mfplay::core
{

enum class VideoCodec
{
    H264,
    H265,
};

[[nodiscard]] constexpr std::string_view codecName(VideoCodec codec)
{
    return codec == VideoCodec::H265 ? "H.265" : "H.264";
}

} // namespace mfplay::core
