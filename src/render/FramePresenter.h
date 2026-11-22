#pragma once

#include "render/Layout.h"
#include "render/VideoFrame.h"

#include <cstddef>
#include <expected>
#include <string>
#include <vector>

#include <d3d12.h>
#include <wrl/client.h>

namespace lynx::render
{

// Which RGB picture the scaling pass draws: the shader's YUV-to-RGB
// intermediate, or the video processor's output (VSR).
enum class ScaledPicture
{
    Converted,
    VideoSuperResolution,
};

// The intermediate is BGRA8 so NVIDIA's optical flow engine can read it
// directly (it takes no RGBA8 input).
inline constexpr DXGI_FORMAT kIntermediateFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

// Draws VideoFrames in two passes: YUV to RGB into an intermediate picture,
// then the picture scaled to fit the back buffer with black bars around it.
// The stages are separate so the pipeline can time each one.
class FramePresenter
{
public:
    // Keeps a set of views and an intermediate picture per back buffer, since
    // each frame in flight reads and writes its own.
    [[nodiscard]] static std::expected<FramePresenter, std::string> create(
        ID3D12Device& device, UINT bufferCount, DXGI_FORMAT targetFormat,
        DXGI_FORMAT intermediateFormat = kIntermediateFormat);

    // Whether the decoded picture's size differs from the intermediate's.
    [[nodiscard]] bool needsPicture(SurfaceSize picture) const;

    // Recreates the intermediate pictures. The GPU must be idle.
    [[nodiscard]] std::expected<void, std::string> resizePicture(SurfaceSize picture);

    // The converted picture of a back buffer; optical flow reads it.
    [[nodiscard]] ID3D12Resource* intermediate(UINT bufferIndex) const;
    [[nodiscard]] DXGI_FORMAT intermediateFormat() const;

    // Points this buffer's VSR view at the video processor's output texture.
    void writeVideoSuperResolutionView(UINT bufferIndex, ID3D12Resource& texture);

    // YUV to RGB into this buffer's intermediate picture.
    void recordConversion(ID3D12GraphicsCommandList& commandList, UINT bufferIndex,
                          const VideoFrame& frame);

    // Draws `picture`'s scaled image over the fit rectangle of `targetSize`;
    // `target` is back buffer `bufferIndex`'s render target view, already in
    // the RENDER_TARGET state and cleared to black.
    void recordScaling(ID3D12GraphicsCommandList& commandList, UINT bufferIndex,
                       D3D12_CPU_DESCRIPTOR_HANDLE target, SurfaceSize targetSize,
                       SurfaceSize pictureSize, ScaledPicture picture);

private:
    FramePresenter() = default;

    void writeInputViews(UINT bufferIndex, const VideoFrame& frame);
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE cpuView(UINT index) const;
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE gpuView(UINT index) const;
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE cpuRenderTarget(UINT index) const;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> conversionRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> conversionPipeline_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> scalingRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> scalingPipeline_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> viewHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> intermediateTargetHeap_;
    UINT viewDescriptorSize_ = 0;
    UINT intermediateTargetDescriptorSize_ = 0;
    UINT bufferCount_ = 0;
    DXGI_FORMAT intermediateFormat_ = kIntermediateFormat;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediates_;
    SurfaceSize pictureSize_;
};

} // namespace lynx::render
