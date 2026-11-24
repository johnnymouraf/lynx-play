#include "render/MotionVectors.h"

#include "core/Windows.h"
#include "render/OpticalFlow.h"
#include "render/shaders/FlowDensifyComputeShader.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>

namespace mfplay::render
{

namespace
{

using core::describeFailure;
using Microsoft::WRL::ComPtr;

constexpr UINT kConstantCount = 12;
constexpr UINT kSrvParameter = 1;
constexpr UINT kUavParameter = 2;
constexpr UINT kGroupSize = 16;

struct DensifyConstants
{
    std::uint32_t gridWidth = 0;
    std::uint32_t gridHeight = 0;
    std::uint32_t sourceWidth = 0;
    std::uint32_t sourceHeight = 0;
    std::uint32_t outputWidth = 0;
    std::uint32_t outputHeight = 0;
    std::uint32_t fitLeft = 0;
    std::uint32_t fitTop = 0;
    std::uint32_t fitWidth = 0;
    std::uint32_t fitHeight = 0;
    std::uint32_t gridSize = 0;
    std::uint32_t padding = 0;
};

static_assert(sizeof(DensifyConstants) == kConstantCount * sizeof(std::uint32_t));

D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;

    return barrier;
}

D3D12_RESOURCE_BARRIER unorderedAccessBarrier()
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = nullptr;

    return barrier;
}

std::expected<ComPtr<ID3D12RootSignature>, std::string> createRootSignature(
    ID3D12Device& device)
{
    const D3D12_DESCRIPTOR_RANGE flow{
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        .NumDescriptors = 1,
        .BaseShaderRegister = 0,
        .RegisterSpace = 0,
        .OffsetInDescriptorsFromTableStart = 0,
    };
    const D3D12_DESCRIPTOR_RANGE vectors{
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
        .NumDescriptors = 1,
        .BaseShaderRegister = 0,
        .RegisterSpace = 0,
        .OffsetInDescriptorsFromTableStart = 0,
    };

    std::array<D3D12_ROOT_PARAMETER, 3> parameters{};
    parameters.at(0).ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters.at(0).Constants = {
        .ShaderRegister = 0,
        .RegisterSpace = 0,
        .Num32BitValues = kConstantCount,
    };
    parameters.at(0).ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters.at(kSrvParameter).ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters.at(kSrvParameter).DescriptorTable = {
        .NumDescriptorRanges = 1,
        .pDescriptorRanges = &flow,
    };
    parameters.at(kSrvParameter).ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters.at(kUavParameter).ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters.at(kUavParameter).DescriptorTable = {
        .NumDescriptorRanges = 1,
        .pDescriptorRanges = &vectors,
    };
    parameters.at(kUavParameter).ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    const D3D12_ROOT_SIGNATURE_DESC description{
        .NumParameters = static_cast<UINT>(parameters.size()),
        .pParameters = parameters.data(),
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS,
    };

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;

    if (const HRESULT result = D3D12SerializeRootSignature(
            &description, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
        FAILED(result))
    {
        return std::unexpected(describeFailure("D3D12SerializeRootSignature (motion vectors)",
                                               result));
    }

    ComPtr<ID3D12RootSignature> rootSignature;

    if (const HRESULT result =
            device.CreateRootSignature(0, serialized->GetBufferPointer(),
                                       serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateRootSignature (motion vectors)", result));
    }

    return rootSignature;
}

D3D12_RESOURCE_DESC outputDescription(SurfaceSize picture)
{
    return {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = picture.width,
        .Height = picture.height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_R16G16_FLOAT,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
    };
}

} // namespace

std::expected<MotionVectors, std::string> MotionVectors::create(ID3D12Device& device,
                                                                UINT slotCount)
{
    if (slotCount == 0)
    {
        return std::unexpected("motion vectors need at least one flow slot");
    }

    MotionVectors vectors;
    vectors.device_ = &device;
    vectors.slotCount_ = slotCount;

    auto rootSignature = createRootSignature(device);

    if (!rootSignature)
    {
        return std::unexpected(rootSignature.error());
    }

    vectors.rootSignature_ = std::move(*rootSignature);

    const D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDescription{
        .pRootSignature = vectors.rootSignature_.Get(),
        .CS = {kFlowDensifyComputeShader, sizeof(kFlowDensifyComputeShader)},
    };

    if (const HRESULT result = device.CreateComputePipelineState(
            &pipelineDescription, IID_PPV_ARGS(&vectors.pipeline_));
        FAILED(result))
    {
        return std::unexpected(
            describeFailure("CreateComputePipelineState (motion vectors)", result));
    }

    const D3D12_DESCRIPTOR_HEAP_DESC heapDescription{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = slotCount + 1,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    };

    if (const HRESULT result =
            device.CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&vectors.viewHeap_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateDescriptorHeap (motion vectors)", result));
    }

    vectors.descriptorSize_ =
        device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    return vectors;
}

bool MotionVectors::configuredFor(SurfaceSize picture, SurfaceSize output,
                                  std::span<ID3D12Resource* const> flows) const
{
    if (output_ == nullptr || picture_ != picture || outputSize_ != output ||
        flows.size() != flows_.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < flows.size(); ++index)
    {
        if (flows_.at(index).Get() != flows[index])
        {
            return false;
        }
    }

    return true;
}

std::expected<void, std::string> MotionVectors::configure(
    SurfaceSize picture, SurfaceSize output, std::span<ID3D12Resource* const> flows)
{
    if (picture.width == 0 || picture.height == 0 || output.width == 0 ||
        output.height == 0)
    {
        return std::unexpected("the picture has no size");
    }

    if (flows.size() != slotCount_)
    {
        return std::unexpected(std::format("{} flow pictures for {} slots", flows.size(),
                                           slotCount_));
    }

    bool sameFlows = flows_.size() == flows.size();

    for (std::size_t index = 0; sameFlows && index < flows.size(); ++index)
    {
        sameFlows = flows_.at(index).Get() == flows[index];
    }

    if (output_ != nullptr && picture_ == picture && outputSize_ == output && sameFlows)
    {
        return {};
    }

    const D3D12_HEAP_PROPERTIES defaultHeap{.Type = D3D12_HEAP_TYPE_DEFAULT};
    const D3D12_RESOURCE_DESC description = outputDescription(output);

    ComPtr<ID3D12Resource> texture;

    if (const HRESULT result = device_->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&texture));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommittedResource (motion vectors)", result));
    }

    device_->CreateUnorderedAccessView(texture.Get(), nullptr, nullptr,
                                        cpuDescriptor(slotCount_));

    flows_.clear();
    flows_.reserve(flows.size());

    for (std::size_t index = 0; index < flows.size(); ++index)
    {
        const D3D12_SHADER_RESOURCE_VIEW_DESC view{
            .Format = DXGI_FORMAT_R16G16_SINT,
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
            .Texture2D = {
                .MostDetailedMip = 0,
                .MipLevels = 1,
                .PlaneSlice = 0,
                .ResourceMinLODClamp = 0.0F,
            },
        };

        device_->CreateShaderResourceView(flows[index], &view,
                                          cpuDescriptor(static_cast<UINT>(index)));
        flows_.emplace_back(flows[index]);
    }

    output_ = std::move(texture);
    picture_ = picture;
    outputSize_ = output;
    grid_ = flowGridExtent(picture, kFlowGridSize);
    fit_ = fitPicture(output, picture);
    return {};
}

void MotionVectors::record(ID3D12GraphicsCommandList& commandList, UINT slot)
{
    if (output_ == nullptr || slot >= flows_.size())
    {
        return;
    }

    ID3D12Resource* const flow = flows_.at(slot).Get();
    const auto flowToShader =
        transition(flow, D3D12_RESOURCE_STATE_COMMON,
                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList.ResourceBarrier(1, &flowToShader);

    const auto outputToUnorderedAccess =
        transition(output_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList.ResourceBarrier(1, &outputToUnorderedAccess);

    const DensifyConstants constants{
        .gridWidth = grid_.width,
        .gridHeight = grid_.height,
        .sourceWidth = picture_.width,
        .sourceHeight = picture_.height,
        .outputWidth = outputSize_.width,
        .outputHeight = outputSize_.height,
        .fitLeft = fit_.left,
        .fitTop = fit_.top,
        .fitWidth = fit_.width,
        .fitHeight = fit_.height,
        .gridSize = kFlowGridSize,
        .padding = 0,
    };

    const std::array<ID3D12DescriptorHeap*, 1> heaps{viewHeap_.Get()};
    commandList.SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    commandList.SetComputeRootSignature(rootSignature_.Get());
    commandList.SetComputeRoot32BitConstants(0, kConstantCount, &constants, 0);
    commandList.SetComputeRootDescriptorTable(kSrvParameter, gpuDescriptor(slot));
    commandList.SetComputeRootDescriptorTable(kUavParameter, gpuDescriptor(slotCount_));
    commandList.SetPipelineState(pipeline_.Get());

    const UINT groupsX = (outputSize_.width + kGroupSize - 1) / kGroupSize;
    const UINT groupsY = (outputSize_.height + kGroupSize - 1) / kGroupSize;
    commandList.Dispatch(groupsX, groupsY, 1);

    const D3D12_RESOURCE_BARRIER dispatchBarrier = unorderedAccessBarrier();
    commandList.ResourceBarrier(1, &dispatchBarrier);

    const auto outputToShader =
        transition(output_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList.ResourceBarrier(1, &outputToShader);

    const auto flowToCommon =
        transition(flow, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_COMMON);
    commandList.ResourceBarrier(1, &flowToCommon);
}

ID3D12Resource* MotionVectors::texture() const
{
    return output_.Get();
}

SurfaceSize MotionVectors::extent() const
{
    return outputSize_;
}

D3D12_CPU_DESCRIPTOR_HANDLE MotionVectors::cpuDescriptor(UINT index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = viewHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * descriptorSize_;

    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE MotionVectors::gpuDescriptor(UINT index) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = viewHeap_->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(index) * descriptorSize_;

    return handle;
}

} // namespace mfplay::render
