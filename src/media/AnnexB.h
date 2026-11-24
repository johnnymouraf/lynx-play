#pragma once

#include "core/VideoCodec.h"

#include <cstdint>
#include <span>
#include <vector>

namespace mfplay::media
{

// Splits an Annex B elementary stream (NAL units after 00 00 01 start codes)
// into access units, each holding one picture with the parameter sets and SEI
// in front of it. A picture's slices stay together. Bytes before the first
// start code are skipped.
[[nodiscard]] std::vector<std::span<const std::uint8_t>> splitAccessUnits(
    std::span<const std::uint8_t> stream, core::VideoCodec codec);

} // namespace mfplay::media
