#include "media/AnnexB.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using lynx::media::splitAccessUnits;
using lynx::core::VideoCodec;

namespace
{

using Bytes = std::vector<std::uint8_t>;

Bytes joined(std::initializer_list<Bytes> parts)
{
    Bytes stream;

    for (const Bytes& part : parts)
    {
        stream.insert(stream.end(), part.begin(), part.end());
    }

    return stream;
}

std::vector<Bytes> split(const Bytes& stream, VideoCodec codec)
{
    std::vector<Bytes> units;

    for (const auto unit : splitAccessUnits(stream, codec))
    {
        units.emplace_back(unit.begin(), unit.end());
    }

    return units;
}

// H.264 NAL units: the type is in the low five bits of the first byte, and a
// slice whose next bit is set opens a picture.
const Bytes kH264Sps{0, 0, 0, 1, 0x67, 0x42, 0xC0};
const Bytes kH264Pps{0, 0, 0, 1, 0x68, 0xCE};
const Bytes kH264Idr{0, 0, 0, 1, 0x65, 0x88, 0x84};
const Bytes kH264FirstSlice{0, 0, 1, 0x41, 0x9A, 0x22};
const Bytes kH264SecondSlice{0, 0, 1, 0x41, 0x1A, 0x33};
const Bytes kH264NextPicture{0, 0, 0, 1, 0x41, 0x9A, 0x44};

// H.265 NAL units: the type is in bits 1 to 6 of the first byte, and a slice
// whose third byte starts with a set bit opens a picture.
const Bytes kH265Vps{0, 0, 0, 1, 0x40, 0x01, 0x0C};
const Bytes kH265Sps{0, 0, 0, 1, 0x42, 0x01, 0x01};
const Bytes kH265Pps{0, 0, 0, 1, 0x44, 0x01, 0xC1};
const Bytes kH265Idr{0, 0, 0, 1, 0x26, 0x01, 0xAF};
const Bytes kH265FirstSegment{0, 0, 0, 1, 0x02, 0x01, 0xD0};
const Bytes kH265SecondSegment{0, 0, 0, 1, 0x02, 0x01, 0x50};
const Bytes kH265SuffixSei{0, 0, 0, 1, 0x50, 0x01, 0x05};
const Bytes kH265NextPicture{0, 0, 1, 0x02, 0x01, 0xD4};

} // namespace

TEST_CASE("splitAccessUnits keeps H.264 parameter sets with the picture after them")
{
    const Bytes stream = joined(
        {kH264Sps, kH264Pps, kH264Idr, kH264FirstSlice, kH264SecondSlice, kH264NextPicture});

    const auto units = split(stream, VideoCodec::H264);

    REQUIRE(units.size() == 3U);
    CHECK(units[0] == joined({kH264Sps, kH264Pps, kH264Idr}));
    CHECK(units[1] == joined({kH264FirstSlice, kH264SecondSlice}));
    CHECK(units[2] == kH264NextPicture);
}

TEST_CASE("splitAccessUnits keeps H.265 slice segments and suffix SEI with their picture")
{
    const Bytes stream = joined({kH265Vps, kH265Sps, kH265Pps, kH265Idr, kH265FirstSegment,
                                 kH265SecondSegment, kH265SuffixSei, kH265NextPicture});

    const auto units = split(stream, VideoCodec::H265);

    REQUIRE(units.size() == 3U);
    CHECK(units[0] == joined({kH265Vps, kH265Sps, kH265Pps, kH265Idr}));
    CHECK(units[1] == joined({kH265FirstSegment, kH265SecondSegment, kH265SuffixSei}));
    CHECK(units[2] == kH265NextPicture);
}

TEST_CASE("splitAccessUnits skips bytes before the first start code")
{
    const Bytes stream = joined({{0xFF, 0x12}, kH264Idr});

    const auto units = split(stream, VideoCodec::H264);

    REQUIRE(units.size() == 1U);
    CHECK(units[0] == kH264Idr);
}

TEST_CASE("splitAccessUnits finds nothing without a picture")
{
    CHECK(split({}, VideoCodec::H264).empty());
    CHECK(split({0x01, 0x02, 0x03}, VideoCodec::H265).empty());
    CHECK(split(joined({kH265Vps, kH265Sps}), VideoCodec::H265).empty());
}
