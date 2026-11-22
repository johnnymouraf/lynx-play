#pragma once

#include "render/Hdr.h"
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

// The SDR intermediate is BGRA8 so NVIDIA's optical flow engine can read it
// directly (it takes no RGBA8 input). A PQ stream needs R10G10B10A2 for its
// picture, so its flow reads a BGRA8 copy of the intermediate instead.
inline constexpr DXGI_FORMAT kIntermediateFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

// Draws VideoFrames in two passes: YUV to RGB into an intermediate picture,
// then the picture scaled to fit the back buffer with black bars around it.
// The stages are separate so the pipeline can time each one.
class FramePresenter
{
public:
    // Keeps a set of views and an intermediate picture per back buffer, since
    // each frame in flight reads and writes its own. The intermediate holds
    // the converted picture, at 10 bits when it carries PQ; the target is the
    // back buffer's format. An intermediate that isn't BGRA8 also gets a
    // BGRA8 copy per buffer for optical flow.
    [[nodiscard]] static std::expected<FramePresenter, std::string> create(
        ID3D12Device& device, UINT bufferCount, DXGI_FORMAT intermediateFormat,
        DXGI_FORMAT targetFormat);

    // Whether the decoded picture's size differs from the intermediate's.
    [[nodiscard]] bool needsPicture(SurfaceSize picture) const;

    // Recreates the intermediate pictures. The GPU must be idle.
    [[nodiscard]] std::expected<void, std::string> resizePicture(SurfaceSize picture);

    // The converted picture of a back buffer.
    [[nodiscard]] ID3D12Resource* intermediate(UINT bufferIndex) const;
    [[nodiscard]] DXGI_FORMAT intermediateFormat() const;

    // The BGRA8 picture the optical flow engine reads: the intermediate itself
    // when it is BGRA8, otherwise the copy recordFlowPicture fills.
    [[nodiscard]] ID3D12Resource* flowIntermediate(UINT bufferIndex) const;

    // Points this buffer's VSR view at the video processor's output texture.
    void writeVideoSuperResolutionView(UINT bufferIndex, ID3D12Resource& texture);

    // YUV to RGB into this buffer's intermediate picture.
    void recordConversion(ID3D12GraphicsCommandList& commandList, UINT bufferIndex,
                          const VideoFrame& frame);

    // The BGRA8 copy for optical flow, drawn from the intermediate recorded
    // this frame; a tone map turns a PQ intermediate into SDR. Nothing when
    // the intermediate is already BGRA8.
    void recordFlowPicture(ID3D12GraphicsCommandList& commandList, UINT bufferIndex,
                           const ToneMapParameters& toneMap);

    // Draws `picture`'s scaled image over the fit rectangle of `targetSize`;
    // `target` is back buffer `bufferIndex`'s render target view, already in
    // the RENDER_TARGET state and cleared to black. A tone map converts a PQ
    // intermediate to the SDR gamma target.
    void recordScaling(ID3D12GraphicsCommandList& commandList, UINT bufferIndex,
                       D3D12_CPU_DESCRIPTOR_HANDLE target, SurfaceSize targetSize,
                       SurfaceSize pictureSize, ScaledPicture picture,
                       const ToneMapParameters& toneMap);

private:
    FramePresenter() = default;

    [[nodiscard]] bool needsFlowCopy() const;
    void drawPicture(ID3D12GraphicsCommandList& commandList, UINT bufferIndex,
                     ID3D12PipelineState& pipeline, D3D12_CPU_DESCRIPTOR_HANDLE target,
                     PixelRect area, UINT pictureView, const ToneMapParameters& toneMap);
    void writeInputViews(UINT bufferIndex, const VideoFrame& frame);
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE cpuView(UINT index) const;
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE gpuView(UINT index) const;
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE cpuRenderTarget(UINT index) const;
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE cpuFlowRenderTarget(UINT index) const;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> conversionRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> conversionPipeline_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> scalingRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> scalingPipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> flowScalingPipeline_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> viewHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> intermediateTargetHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> flowTargetHeap_;
    UINT viewDescriptorSize_ = 0;
    UINT intermediateTargetDescriptorSize_ = 0;
    UINT flowTargetDescriptorSize_ = 0;
    UINT bufferCount_ = 0;
    DXGI_FORMAT intermediateFormat_ = kIntermediateFormat;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediates_;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> flowPictures_;
    SurfaceSize pictureSize_;
};

} // namespace lynx::render
