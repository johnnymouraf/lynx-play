#include "render/CadenceMetric.h"

#include "render/Layout.h"
#include "render/shaders/CadenceComputeShader.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace mfplay::render
{

namespace
{

using core::describeFailure;
using Microsoft::WRL::ComPtr;

constexpr UINT kGroupSize = 64;
constexpr UINT kConstantCount = 8;
constexpr UINT kDescriptorsPerSlot = 3;
constexpr UINT kSrvParameter = 1;
constexpr UINT kUavParameter = 2;
constexpr std::uint32_t kCounterCount = 4;

struct MetricConstants
{
    std::uint32_t blockWidth = 0;
    std::uint32_t blockHeight = 0;
    std::uint32_t pictureWidth = 0;
    std::uint32_t pictureHeight = 0;
    float levelThreshold = 0.0F;
    std::array<std::uint32_t, 3> padding{};
};

static_assert(sizeof(MetricConstants) == kConstantCount * sizeof(std::uint32_t));

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
    const D3D12_DESCRIPTOR_RANGE luma{
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        .NumDescriptors = 2,
        .BaseShaderRegister = 0,
        .RegisterSpace = 0,
        .OffsetInDescriptorsFromTableStart = 0,
    };
    const D3D12_DESCRIPTOR_RANGE counters{
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
        .pDescriptorRanges = &luma,
    };
    parameters.at(kSrvParameter).ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters.at(kUavParameter).ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters.at(kUavParameter).DescriptorTable = {
        .NumDescriptorRanges = 1,
        .pDescriptorRanges = &counters,
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
        return std::unexpected(describeFailure("D3D12SerializeRootSignature (cadence)", result));
    }

    ComPtr<ID3D12RootSignature> rootSignature;

    if (const HRESULT result =
            device.CreateRootSignature(0, serialized->GetBufferPointer(),
                                       serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateRootSignature (cadence)", result));
    }

    return rootSignature;
}

std::expected<ComPtr<ID3D12PipelineState>, std::string> createPipeline(
    ID3D12Device& device, ID3D12RootSignature& rootSignature)
{
    const D3D12_COMPUTE_PIPELINE_STATE_DESC description{
        .pRootSignature = &rootSignature,
        .CS = {kCadenceComputeShader, sizeof(kCadenceComputeShader)},
    };

    ComPtr<ID3D12PipelineState> pipeline;

    if (const HRESULT result =
            device.CreateComputePipelineState(&description, IID_PPV_ARGS(&pipeline));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateComputePipelineState (cadence)", result));
    }

    return pipeline;
}

D3D12_RESOURCE_DESC counterDescription(bool unorderedAccess)
{
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = kCounterCount * sizeof(std::uint32_t);
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = DXGI_FORMAT_UNKNOWN;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    description.Flags =
        unorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

    return description;
}

} // namespace

std::expected<CadenceMetric, std::string> CadenceMetric::create(ID3D12Device& device)
{
    CadenceMetric metric;
    metric.device_ = &device;

    auto rootSignature = createRootSignature(device);

    if (!rootSignature)
    {
        return std::unexpected(rootSignature.error());
    }

    metric.rootSignature_ = std::move(*rootSignature);

    auto pipeline = createPipeline(device, *metric.rootSignature_.Get());

    if (!pipeline)
    {
        return std::unexpected(pipeline.error());
    }

    metric.pipeline_ = std::move(*pipeline);

    const D3D12_DESCRIPTOR_HEAP_DESC heapDescription{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = kSlotCount * kDescriptorsPerSlot,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    };

    if (const HRESULT result =
            device.CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&metric.viewHeap_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateDescriptorHeap (cadence)", result));
    }

    metric.descriptorSize_ =
        device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    const D3D12_HEAP_PROPERTIES defaultHeap{.Type = D3D12_HEAP_TYPE_DEFAULT};
    const D3D12_HEAP_PROPERTIES readbackHeap{.Type = D3D12_HEAP_TYPE_READBACK};
    const D3D12_RESOURCE_DESC counters = counterDescription(true);
    const D3D12_RESOURCE_DESC readback = counterDescription(false);

    for (UINT slot = 0; slot < kSlotCount; ++slot)
    {
        Slot& resources = metric.slots_.at(slot);

        if (const HRESULT result = device.CreateCommittedResource(
                &defaultHeap, D3D12_HEAP_FLAG_NONE, &counters, D3D12_RESOURCE_STATE_COMMON,
                nullptr, IID_PPV_ARGS(&resources.counters));
            FAILED(result))
        {
            return std::unexpected(describeFailure("CreateCommittedResource (cadence)", result));
        }

        if (const HRESULT result = device.CreateCommittedResource(
                &readbackHeap, D3D12_HEAP_FLAG_NONE, &readback, D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr, IID_PPV_ARGS(&resources.readback));
            FAILED(result))
        {
            return std::unexpected(
                describeFailure("CreateCommittedResource (cadence readback)", result));
        }

        D3D12_CPU_DESCRIPTOR_HANDLE handle = metric.viewHeap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(slot * kDescriptorsPerSlot + 2) * metric.descriptorSize_;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
        uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uav.Buffer = {
            .FirstElement = 0,
            .NumElements = kCounterCount,
            .StructureByteStride = sizeof(std::uint32_t),
            .CounterOffsetInBytes = 0,
            .Flags = D3D12_BUFFER_UAV_FLAG_NONE,
        };

        device.CreateUnorderedAccessView(resources.counters.Get(), nullptr, &uav, handle);
    }

    if (const HRESULT result =
            device.CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&metric.fence_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateFence (cadence)", result));
    }

    metric.samplesReadyEvent_.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));

    if (!metric.samplesReadyEvent_)
    {
        return std::unexpected(
            describeFailure("CreateEventW (cadence)", HRESULT_FROM_WIN32(GetLastError())));
    }

    return metric;
}

std::uint64_t CadenceMetric::record(ID3D12GraphicsCommandList& commandList,
                                    const VideoFrame& current, const VideoFrame& previous)
{
    if (current.format != previous.format || current.width != previous.width ||
        current.height != previous.height || current.width == 0 || current.height == 0)
    {
        return 0;
    }

    const std::optional<UINT> slot = freeSlot();

    if (!slot)
    {
        return 0;
    }

    writeLumaView(*slot, true, current);
    writeLumaView(*slot, false, previous);

    Slot& resources = slots_.at(*slot);

    const auto toUnorderedAccess =
        transition(resources.counters.Get(), D3D12_RESOURCE_STATE_COMMON,
                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList.ResourceBarrier(1, &toUnorderedAccess);

    const std::array<std::uint32_t, 4> zeros{};
    D3D12_CPU_DESCRIPTOR_HANDLE cpu =
        viewHeap_->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += static_cast<SIZE_T>(*slot * kDescriptorsPerSlot + 2) * descriptorSize_;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu =
        viewHeap_->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += static_cast<UINT64>(*slot * kDescriptorsPerSlot + 2) * descriptorSize_;

    commandList.ClearUnorderedAccessViewUint(gpu, cpu, resources.counters.Get(),
                                             zeros.data(), 0, nullptr);

    const D3D12_RESOURCE_BARRIER clearBarrier = unorderedAccessBarrier();
    commandList.ResourceBarrier(1, &clearBarrier);

    const std::array<ID3D12DescriptorHeap*, 1> heaps{viewHeap_.Get()};
    commandList.SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    commandList.SetComputeRootSignature(rootSignature_.Get());

    const SurfaceSize grid = {
        .width = (current.width + kGridSize - 1) / kGridSize,
        .height = (current.height + kGridSize - 1) / kGridSize,
    };
    const MetricConstants constants{
        .blockWidth = grid.width,
        .blockHeight = grid.height,
        .pictureWidth = current.width,
        .pictureHeight = current.height,
        .levelThreshold = kLevelThreshold,
        .padding = {},
    };

    commandList.SetComputeRoot32BitConstants(0, kConstantCount, &constants, 0);

    D3D12_GPU_DESCRIPTOR_HANDLE luma =
        viewHeap_->GetGPUDescriptorHandleForHeapStart();
    luma.ptr += static_cast<UINT64>(*slot * kDescriptorsPerSlot) * descriptorSize_;
    commandList.SetComputeRootDescriptorTable(kSrvParameter, luma);
    commandList.SetComputeRootDescriptorTable(kUavParameter, gpu);
    commandList.SetPipelineState(pipeline_.Get());

    const std::uint32_t blocks = grid.width * grid.height;
    const UINT groups = (blocks + kGroupSize - 1) / kGroupSize;
    commandList.Dispatch(groups, 1, 1);

    const D3D12_RESOURCE_BARRIER dispatchBarrier = unorderedAccessBarrier();
    commandList.ResourceBarrier(1, &dispatchBarrier);

    const auto toCopySource =
        transition(resources.counters.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                   D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList.ResourceBarrier(1, &toCopySource);
    commandList.CopyBufferRegion(resources.readback.Get(), 0, resources.counters.Get(), 0,
                                 sizeof(CadenceReadback));

    const auto toCommon =
        transition(resources.counters.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                   D3D12_RESOURCE_STATE_COMMON);
    commandList.ResourceBarrier(1, &toCommon);

    resources.pending = ++nextFenceValue_;
    return resources.pending;
}

std::optional<CadenceReadback> CadenceMetric::take()
{
    for (Slot& slot : slots_)
    {
        if (slot.pending == 0 || fence_->GetCompletedValue() < slot.pending)
        {
            continue;
        }

        void* mapped = nullptr;
        const D3D12_RANGE range{.Begin = 0, .End = sizeof(CadenceReadback)};

        if (const HRESULT result = slot.readback->Map(0, &range, &mapped); FAILED(result))
        {
            slot.pending = 0;
            continue;
        }

        CadenceReadback sample;
        std::memcpy(&sample, mapped, sizeof(sample));
        slot.readback->Unmap(0, nullptr);
        slot.pending = 0;

        return sample;
    }

    return std::nullopt;
}

std::optional<CadenceReadback> CadenceMetric::wait(std::uint64_t fenceValue)
{
    constexpr DWORD kWaitTimeoutMilliseconds = 100;

    if (fenceValue == 0)
    {
        return std::nullopt;
    }

    if (fence_->GetCompletedValue() < fenceValue)
    {
        if (samplesReadyEvent_ == nullptr)
        {
            return std::nullopt;
        }

        if (const HRESULT result =
                fence_->SetEventOnCompletion(fenceValue, samplesReadyEvent_.get());
            FAILED(result))
        {
            return std::nullopt;
        }

        if (WaitForSingleObject(samplesReadyEvent_.get(), kWaitTimeoutMilliseconds) !=
            WAIT_OBJECT_0)
        {
            return std::nullopt;
        }
    }

    for (Slot& slot : slots_)
    {
        if (slot.pending != fenceValue)
        {
            continue;
        }

        void* mapped = nullptr;
        const D3D12_RANGE range{.Begin = 0, .End = sizeof(CadenceReadback)};

        if (const HRESULT result = slot.readback->Map(0, &range, &mapped); FAILED(result))
        {
            slot.pending = 0;
            return std::nullopt;
        }

        CadenceReadback sample;
        std::memcpy(&sample, mapped, sizeof(sample));
        slot.readback->Unmap(0, nullptr);
        slot.pending = 0;

        return sample;
    }

    return std::nullopt;
}

ID3D12Fence* CadenceMetric::fence() const
{
    return fence_.Get();
}

std::optional<UINT> CadenceMetric::freeSlot()
{
    for (UINT slot = 0; slot < kSlotCount; ++slot)
    {
        // A slot still holding a sample waits for take(), so the completed
        // readback is never overwritten before it is read.
        if (slots_.at(slot).pending == 0)
        {
            return slot;
        }
    }

    return std::nullopt;
}

void CadenceMetric::writeLumaView(UINT slot, bool current, const VideoFrame& frame)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = viewHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr +=
        static_cast<SIZE_T>(slot * kDescriptorsPerSlot + (current ? 0 : 1)) * descriptorSize_;

    const DXGI_FORMAT format =
        frame.format == PixelFormat::P010 ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;

    const D3D12_SHADER_RESOURCE_VIEW_DESC view{
        .Format = format,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2DArray = {
            .MostDetailedMip = 0,
            .MipLevels = 1,
            .FirstArraySlice = frame.arraySlice,
            .ArraySize = 1,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0.0F,
        },
    };

    device_->CreateShaderResourceView(frame.texture.Get(), &view, handle);
}

} // namespace mfplay::render
