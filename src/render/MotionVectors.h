#pragma once

#include "render/Layout.h"

#include <expected>
#include <span>
#include <string>
#include <vector>

#include <d3d12.h>
#include <wrl/client.h>

namespace mfplay::render
{

// NVIDIA's coarse flow grid as a full-resolution current-to-previous vector
// picture for DLSS. The consumer keeps the output in
// D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, which is the state NGX
// reads its motion vectors in.
class MotionVectors
{
public:
    [[nodiscard]] static std::expected<MotionVectors, std::string> create(
        ID3D12Device& device, UINT slotCount);

    MotionVectors(const MotionVectors&) = delete;
    MotionVectors& operator=(const MotionVectors&) = delete;
    MotionVectors(MotionVectors&&) = default;
    MotionVectors& operator=(MotionVectors&&) = default;
    ~MotionVectors() = default;

    // Whether the output is built for this source picture and output size for
    // these flow grids.
    [[nodiscard]] bool configuredFor(SurfaceSize picture, SurfaceSize output,
                                      std::span<ID3D12Resource* const> flows) const;

    // Builds the output and a view of each flow grid. `picture` is the
    // decoded size the flow was computed at; `output` is the back buffer,
    // where the source is drawn letterboxed. The GPU must be idle: the old
    // output is released.
    [[nodiscard]] std::expected<void, std::string> configure(
        SurfaceSize picture, SurfaceSize output, std::span<ID3D12Resource* const> flows);

    // Densifies one slot's flow grid; the flow resource must already have
    // completed (the caller waits on the flow engine's fence first).
    void record(ID3D12GraphicsCommandList& commandList, UINT slot);

    [[nodiscard]] ID3D12Resource* texture() const;
    [[nodiscard]] SurfaceSize extent() const;

private:
    MotionVectors() = default;

    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor(UINT index) const;
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor(UINT index) const;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> viewHeap_;
    UINT descriptorSize_ = 0;
    UINT slotCount_ = 0;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> flows_;
    Microsoft::WRL::ComPtr<ID3D12Resource> output_;
    SurfaceSize picture_;
    SurfaceSize outputSize_;
    SurfaceSize grid_;
    PixelRect fit_;
};

} // namespace mfplay::render
