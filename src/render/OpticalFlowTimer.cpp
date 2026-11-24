#include "render/OpticalFlowTimer.h"

#include <array>
#include <cstring>

namespace mfplay::render
{

namespace
{

using core::describeFailure;

constexpr UINT kStampsPerSlot = 2;
constexpr UINT kInputStamp = 0;
constexpr UINT kOutputStamp = 1;

} // namespace

std::expected<OpticalFlowTimer, std::string> OpticalFlowTimer::create(ID3D12Device& device,
                                                                      UINT slotCount)
{
    OpticalFlowTimer timer;
    timer.slots_.resize(slotCount);
    timer.allocators_.resize(slotCount);

    const D3D12_COMMAND_QUEUE_DESC queueDescription{.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE};

    if (const HRESULT result =
            device.CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&timer.queue_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommandQueue (flow timer)", result));
    }

    if (const HRESULT result =
            device.CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&timer.fence_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateFence (flow timer)", result));
    }

    for (auto& allocator : timer.allocators_)
    {
        if (const HRESULT result = device.CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&allocator));
            FAILED(result))
        {
            return std::unexpected(describeFailure("CreateCommandAllocator (flow timer)", result));
        }
    }

    if (const HRESULT result = device.CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_COMPUTE, timer.allocators_.front().Get(), nullptr,
            IID_PPV_ARGS(&timer.commandList_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommandList (flow timer)", result));
    }

    timer.commandList_->Close();

    const D3D12_QUERY_HEAP_DESC heapDescription{
        .Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
        .Count = slotCount * kStampsPerSlot,
    };

    if (const HRESULT result =
            device.CreateQueryHeap(&heapDescription, IID_PPV_ARGS(&timer.queryHeap_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateQueryHeap (flow timer)", result));
    }

    const D3D12_RESOURCE_DESC bufferDescription{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = static_cast<UINT64>(slotCount) * kStampsPerSlot * sizeof(std::uint64_t),
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
    };
    const D3D12_HEAP_PROPERTIES readbackHeap{.Type = D3D12_HEAP_TYPE_READBACK};

    if (const HRESULT result = device.CreateCommittedResource(
            &readbackHeap, D3D12_HEAP_FLAG_NONE, &bufferDescription,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&timer.readback_));
        FAILED(result))
    {
        return std::unexpected(
            describeFailure("CreateCommittedResource (flow timer timestamps)", result));
    }

    if (const HRESULT result = timer.queue_->GetTimestampFrequency(&timer.frequency_);
        FAILED(result))
    {
        return std::unexpected(describeFailure("GetTimestampFrequency (flow timer)", result));
    }

    return timer;
}

OpticalFlowTimer::~OpticalFlowTimer()
{
    drain();
}

std::expected<void, std::string> OpticalFlowTimer::submit(UINT slot, ID3D12Fence& flowFence,
                                                          std::uint64_t inputValue,
                                                          std::uint64_t outputValue)
{
    Slot& state = slots_.at(slot);

    // The slot's allocator is reused: the stamps of its last flow must be done.
    if (fence_->GetCompletedValue() < state.fenceValue)
    {
        fence_->SetEventOnCompletion(state.fenceValue, nullptr);
    }

    allocators_.at(slot)->Reset();

    queue_->Wait(&flowFence, inputValue);

    if (auto stamped = recordStamp(slot, kInputStamp, false); !stamped)
    {
        return stamped;
    }

    queue_->Wait(&flowFence, outputValue);

    if (auto stamped = recordStamp(slot, kOutputStamp, true); !stamped)
    {
        return stamped;
    }

    state.fenceValue = ++lastValue_;
    state.pending = true;
    queue_->Signal(fence_.Get(), state.fenceValue);
    return {};
}

std::optional<double> OpticalFlowTimer::take(UINT slot)
{
    Slot& state = slots_.at(slot);

    if (!state.pending || fence_->GetCompletedValue() < state.fenceValue)
    {
        return std::nullopt;
    }

    state.pending = false;

    const std::size_t offset =
        static_cast<std::size_t>(slot) * kStampsPerSlot * sizeof(std::uint64_t);
    const D3D12_RANGE readRange{
        .Begin = offset,
        .End = offset + kStampsPerSlot * sizeof(std::uint64_t),
    };
    void* mapped = nullptr;

    if (FAILED(readback_->Map(0, &readRange, &mapped)))
    {
        return std::nullopt;
    }

    std::array<std::uint64_t, kStampsPerSlot> ticks{};
    std::memcpy(ticks.data(), static_cast<const std::uint8_t*>(mapped) + offset,
                sizeof(ticks));
    readback_->Unmap(0, nullptr);

    const std::uint64_t begin = ticks.at(kInputStamp);
    const std::uint64_t end = ticks.at(kOutputStamp);

    if (frequency_ == 0 || begin == 0 || end < begin)
    {
        return std::nullopt;
    }

    return static_cast<double>(end - begin) * 1000.0 / static_cast<double>(frequency_);
}

void OpticalFlowTimer::drain()
{
    if (fence_ == nullptr || fence_->GetCompletedValue() >= lastValue_)
    {
        return;
    }

    fence_->SetEventOnCompletion(lastValue_, nullptr);
}

std::expected<void, std::string> OpticalFlowTimer::recordStamp(UINT slot, UINT query, bool resolve)
{
    commandList_->Reset(allocators_.at(slot).Get(), nullptr);

    const UINT first = slot * kStampsPerSlot;
    commandList_->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first + query);

    if (resolve)
    {
        commandList_->ResolveQueryData(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first,
                                       kStampsPerSlot, readback_.Get(),
                                       static_cast<UINT64>(first) * sizeof(std::uint64_t));
    }

    if (const HRESULT result = commandList_->Close(); FAILED(result))
    {
        return std::unexpected(describeFailure("ID3D12GraphicsCommandList::Close (flow timer)",
                                               result));
    }

    const std::array<ID3D12CommandList*, 1> lists{commandList_.Get()};
    queue_->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());
    return {};
}

} // namespace mfplay::render
