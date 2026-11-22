#include "render/GpuTimer.h"

#include <cstring>

namespace lynx::render
{

namespace
{

using core::describeFailure;

constexpr std::size_t kTotalBegin = 0;
constexpr std::size_t kTotalEnd = kTimestampCount - 1;

std::size_t stageBegin(EffectStage stage)
{
    return 1 + 2 * static_cast<std::size_t>(stage);
}

} // namespace

std::string_view stageName(EffectStage stage)
{
    switch (stage)
    {
    case EffectStage::ColorConversion:
        return "color conversion";
    case EffectStage::VideoSuperResolution:
        return "VSR";
    case EffectStage::Scaling:
        return "scaling";
    case EffectStage::Overlay:
        return "overlay";
    }

    return "unknown";
}

std::optional<FrameGpuTimes> frameGpuTimes(const FrameTimestamps& timestamps,
                                            std::uint64_t frequency)
{
    if (frequency == 0 || timestamps.ticks.at(kTotalBegin) == 0)
    {
        return std::nullopt;
    }

    const auto milliseconds = [frequency](std::uint64_t end, std::uint64_t begin)
        -> std::optional<double>
    {
        if (begin == 0 || end < begin)
        {
            return std::nullopt;
        }

        return static_cast<double>(end - begin) * 1000.0 / static_cast<double>(frequency);
    };

    FrameGpuTimes times;

    for (std::size_t stage = 0; stage < kEffectStageCount; ++stage)
    {
        const auto elapsed =
            milliseconds(timestamps.ticks.at(stageBegin(static_cast<EffectStage>(stage)) + 1),
                         timestamps.ticks.at(stageBegin(static_cast<EffectStage>(stage))));

        if (!elapsed)
        {
            return std::nullopt;
        }

        times.stages.at(stage) = *elapsed;
    }

    const auto total = milliseconds(timestamps.ticks.at(kTotalEnd),
                                    timestamps.ticks.at(kTotalBegin));

    if (!total)
    {
        return std::nullopt;
    }

    times.total = *total;
    return times;
}

std::expected<GpuTimer, std::string> GpuTimer::create(ID3D12Device& device,
                                                       ID3D12CommandQueue& queue,
                                                       UINT bufferCount)
{
    GpuTimer timer;
    timer.bufferCount_ = bufferCount;
    timer.recorded_.assign(bufferCount, false);

    const D3D12_QUERY_HEAP_DESC heapDescription{
        .Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
        .Count = bufferCount * static_cast<UINT>(kTimestampCount),
    };

    if (const HRESULT result =
            device.CreateQueryHeap(&heapDescription, IID_PPV_ARGS(&timer.queryHeap_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateQueryHeap", result));
    }

    const D3D12_RESOURCE_DESC bufferDescription{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = static_cast<UINT64>(bufferCount) * kTimestampCount * sizeof(std::uint64_t),
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
        return std::unexpected(describeFailure("CreateCommittedResource (timestamps)", result));
    }

    if (const HRESULT result = queue.GetTimestampFrequency(&timer.frequency_); FAILED(result))
    {
        return std::unexpected(describeFailure("GetTimestampFrequency", result));
    }

    return timer;
}

void GpuTimer::beginFrame(ID3D12GraphicsCommandList& commandList, UINT bufferIndex)
{
    stamp(commandList, bufferIndex, kTotalBegin);
}

void GpuTimer::beginStage(ID3D12GraphicsCommandList& commandList, UINT bufferIndex,
                           EffectStage stage)
{
    stamp(commandList, bufferIndex, stageBegin(stage));
}

void GpuTimer::endStage(ID3D12GraphicsCommandList& commandList, UINT bufferIndex,
                        EffectStage stage)
{
    stamp(commandList, bufferIndex, stageBegin(stage) + 1);
}

void GpuTimer::endFrame(ID3D12GraphicsCommandList& commandList, UINT bufferIndex)
{
    stamp(commandList, bufferIndex, kTotalEnd);

    const UINT first = slot(bufferIndex, 0);

    commandList.ResolveQueryData(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first,
                                  static_cast<UINT>(kTimestampCount), readback_.Get(),
                                  static_cast<UINT64>(first) * sizeof(std::uint64_t));
    recorded_.at(bufferIndex) = true;
}

std::optional<FrameGpuTimes> GpuTimer::take(UINT bufferIndex)
{
    if (bufferIndex >= bufferCount_ || !recorded_.at(bufferIndex))
    {
        return std::nullopt;
    }

    const std::size_t offset = static_cast<std::size_t>(slot(bufferIndex, 0)) *
                                sizeof(std::uint64_t);

    void* mapped = nullptr;
    const D3D12_RANGE readRange{
        .Begin = offset,
        .End = offset + sizeof(FrameTimestamps),
    };

    if (const HRESULT result = readback_->Map(0, &readRange, &mapped); FAILED(result))
    {
        return std::nullopt;
    }

    FrameTimestamps timestamps;
    std::memcpy(&timestamps, static_cast<const std::uint8_t*>(mapped) + offset,
                sizeof(timestamps));
    readback_->Unmap(0, nullptr);

    recorded_.at(bufferIndex) = false;
    return frameGpuTimes(timestamps, frequency_);
}

UINT GpuTimer::slot(UINT bufferIndex, std::size_t offset) const
{
    return bufferIndex * static_cast<UINT>(kTimestampCount) + static_cast<UINT>(offset);
}

void GpuTimer::stamp(ID3D12GraphicsCommandList& commandList, UINT bufferIndex, std::size_t offset)
{
    commandList.EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
                         slot(bufferIndex, offset));
}

} // namespace lynx::render
