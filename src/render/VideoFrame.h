#pragma once

#include <chrono>
#include <cstdint>
#include <memory>

#include <d3d12.h>
#include <wrl/client.h>

namespace lynx::render
{

// How a YUV picture's planes are laid out in its texture.
enum class PixelFormat
{
    // 8-bit 4:2:0: a luma plane and an interleaved CbCr plane.
    Nv12,
    // 10-bit NV12, each sample in the high bits of 16.
    P010,
};

// The matrix that turns the picture's YCbCr into RGB.
enum class ColorMatrix
{
    Bt601,
    Bt709,
    Bt2020,
};

enum class ColorRange
{
    // Luma from 16 to 235 and chroma from 16 to 240 (8-bit), as video uses.
    Limited,
    Full,
};

// The picture's transfer function: how its RGB codes map to light.
enum class TransferFunction
{
    // BT.709 (or BT.1886) gamma, as SDR video uses.
    Sdr,
    // SMPTE ST.2084, as HDR10 uses.
    Pq,
};

enum class ColorPrimaries
{
    Bt709,
    Bt2020,
};

// The stream's HDR10 metadata, when it carries it; zero means unknown.
struct HdrMetadata
{
    // MaxCLL and MaxFALL from the content light level information.
    double maxContentLightNits = 0.0;
    double maxFrameAverageNits = 0.0;
    // The mastering display's peak luminance.
    double maxMasteringLuminanceNits = 0.0;

    bool operator==(const HdrMetadata&) const = default;
};

// A picture on the renderer's GPU, ready to draw. Every video source (the test
// pattern, the decoder fed by a file or the PS5, later a capture card) hands
// these to the renderer through a FrameMailbox.
struct VideoFrame
{
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    // The slice of `texture` holding the picture, when it is a texture array.
    std::uint32_t arraySlice = 0;
    PixelFormat format = PixelFormat::Nv12;
    ColorMatrix matrix = ColorMatrix::Bt709;
    ColorRange range = ColorRange::Limited;
    TransferFunction transfer = TransferFunction::Sdr;
    ColorPrimaries primaries = ColorPrimaries::Bt709;
    HdrMetadata hdr;
    // The visible picture, from the texture's top-left corner; the texture may
    // be larger (codecs pad to whole macroblocks).
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // The GPU work that writes the picture; the renderer's queue waits for the
    // fence to reach this value before reading it.
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    std::uint64_t fenceValue = 0;
    // When the source finished the picture.
    std::chrono::steady_clock::time_point timestamp;
    // Holds the source's storage for the picture (a decoder surface) until the
    // renderer's GPU work no longer reads it.
    std::shared_ptr<const void> lease;
};

} // namespace lynx::render
