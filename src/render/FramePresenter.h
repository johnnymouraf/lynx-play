#pragma once

#include "render/Layout.h"
#include "render/VideoFrame.h"

#include <expected>
#include <string>

#include <d3d12.h>
#include <wrl/client.h>

namespace lynx::render
{

// Draws VideoFrames into back buffers: YUV to RGB in a pixel shader, scaled to
// fit the target with black bars around the picture.
class FramePresenter
{
public:
    // Keeps a set of views per back buffer, since each frame in flight reads
    // its own picture.
    [[nodiscard]] static std::expected<FramePresenter, std::string> create(
        ID3D12Device& device, UINT bufferCount, DXGI_FORMAT targetFormat);

    // `target` is back buffer `bufferIndex`'s render target view, already in
    // the RENDER_TARGET state and cleared to black.
    void record(ID3D12GraphicsCommandList& commandList, UINT bufferIndex, const VideoFrame& frame,
                D3D12_CPU_DESCRIPTOR_HANDLE target, SurfaceSize targetSize);

private:
    FramePresenter() = default;

    void writeViews(UINT bufferIndex, const VideoFrame& frame);
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE cpuView(UINT index) const;
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE gpuView(UINT index) const;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> viewHeap_;
    UINT viewDescriptorSize_ = 0;
};

} // namespace lynx::render
