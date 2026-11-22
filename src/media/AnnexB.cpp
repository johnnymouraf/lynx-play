#include "media/AnnexB.h"

namespace lynx::media
{

namespace
{

constexpr std::size_t kStartCodeSize = 3;

// A NAL unit's place in the stream: where its start code begins (with the
// leading zero byte of a four-byte one) and where its header begins.
struct NalUnit
{
    std::size_t begin = 0;
    std::size_t header = 0;
};

enum class NalRole
{
    // Parameter sets, SEI or a delimiter: they come before a new picture.
    StartsAccessUnit,
    FirstSliceOfPicture,
    LaterSlice,
    Other,
};

std::vector<NalUnit> findNalUnits(std::span<const std::uint8_t> stream)
{
    std::vector<NalUnit> units;

    for (std::size_t at = 0; at + kStartCodeSize <= stream.size(); ++at)
    {
        const bool isStartCode = stream[at] == 0 && stream[at + 1] == 0 && stream[at + 2] == 1;

        if (!isStartCode)
        {
            continue;
        }

        const bool hasZeroByte = at > 0 && stream[at - 1] == 0;
        units.push_back({.begin = hasZeroByte ? at - 1 : at, .header = at + kStartCodeSize});
        at += kStartCodeSize - 1;
    }

    return units;
}

// The first bit of a slice header says whether the slice opens its picture
// (H.264's first_mb_in_slice is 0, H.265's first_slice_segment_in_pic_flag
// is set).
NalRole sliceRole(std::span<const std::uint8_t> nal, std::size_t headerSize)
{
    if (nal.size() <= headerSize)
    {
        return NalRole::LaterSlice;
    }

    const bool opensPicture = (nal[headerSize] & 0x80) != 0;
    return opensPicture ? NalRole::FirstSliceOfPicture : NalRole::LaterSlice;
}

NalRole h264Role(std::span<const std::uint8_t> nal)
{
    constexpr std::size_t kHeaderSize = 1;

    if (nal.empty())
    {
        return NalRole::Other;
    }

    const int type = nal[0] & 0x1F;
    const bool opensSlice = type == 1 || type == 2 || type == 5;
    const bool continuesSlice = type == 3 || type == 4;
    const bool startsUnit = (type >= 6 && type <= 9) || (type >= 14 && type <= 18);

    if (opensSlice)
    {
        return sliceRole(nal, kHeaderSize);
    }

    if (continuesSlice)
    {
        return NalRole::LaterSlice;
    }

    return startsUnit ? NalRole::StartsAccessUnit : NalRole::Other;
}

NalRole h265Role(std::span<const std::uint8_t> nal)
{
    constexpr std::size_t kHeaderSize = 2;

    if (nal.size() < kHeaderSize)
    {
        return NalRole::Other;
    }

    const int type = (nal[0] >> 1) & 0x3F;
    const bool isSlice = type <= 31;
    const bool startsUnit = (type >= 32 && type <= 35) || type == 39 ||
                            (type >= 41 && type <= 44) || (type >= 48 && type <= 55);

    if (isSlice)
    {
        return sliceRole(nal, kHeaderSize);
    }

    return startsUnit ? NalRole::StartsAccessUnit : NalRole::Other;
}

NalRole roleOf(std::span<const std::uint8_t> nal, core::VideoCodec codec)
{
    return codec == core::VideoCodec::H265 ? h265Role(nal) : h264Role(nal);
}

} // namespace

std::vector<std::span<const std::uint8_t>> splitAccessUnits(std::span<const std::uint8_t> stream,
                                                            core::VideoCodec codec)
{
    const std::vector<NalUnit> units = findNalUnits(stream);
    std::vector<std::span<const std::uint8_t>> accessUnits;

    if (units.empty())
    {
        return accessUnits;
    }

    std::size_t unitBegin = units.front().begin;
    bool hasPicture = false;

    for (std::size_t index = 0; index < units.size(); ++index)
    {
        const NalUnit& unit = units[index];
        const std::size_t end = index + 1 < units.size() ? units[index + 1].begin : stream.size();
        const NalRole role = roleOf(stream.subspan(unit.header, end - unit.header), codec);

        const bool startsNew =
            role == NalRole::StartsAccessUnit || role == NalRole::FirstSliceOfPicture;

        if (startsNew && hasPicture)
        {
            accessUnits.push_back(stream.subspan(unitBegin, unit.begin - unitBegin));
            unitBegin = unit.begin;
            hasPicture = false;
        }

        if (role == NalRole::FirstSliceOfPicture || role == NalRole::LaterSlice)
        {
            hasPicture = true;
        }
    }

    if (hasPicture)
    {
        accessUnits.push_back(stream.subspan(unitBegin));
    }

    return accessUnits;
}

} // namespace lynx::media
