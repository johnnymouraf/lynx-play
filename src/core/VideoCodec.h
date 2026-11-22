#pragma once

#include <string_view>

namespace lynx::core
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

} // namespace lynx::core
