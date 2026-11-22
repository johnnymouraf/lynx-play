#pragma once

#include "core/Windows.h"

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <d3d12.h>
#include <wrl/client.h>

namespace lynx::render
{

using Microsoft::WRL::ComPtr;

// The effects pipeline's stages, in the order a frame records them.
enum class EffectStage : std::uint32_t
{
    ColorConversion = 0,
    VideoSuperResolution = 1,
    Cadence = 2,
    Scaling = 3,
    Overlay = 4,
    // Timed from the main list's end to the flow engine's completion, which
    // arrives on its own fence between frames.
    OpticalFlow = 5,
};

inline constexpr std::size_t kEffectStageCount = 6;
// The frame's first stamp, each stage's two stamps, and its last one.
inline constexpr std::size_t kTimestampCount = 2 + 2 * kEffectStageCount;

[[nodiscard]] std::string_view stageName(EffectStage stage);

// What the GPU took on one frame, in milliseconds.
struct FrameGpuTimes
{
    std::array<double, kEffectStageCount> stages{};
    double total = 0.0;
};

// One frame's raw timestamps, in query ticks.
struct FrameTimestamps
{
    std::array<std::uint64_t, kTimestampCount> ticks{};
};

// Turns a frame's ticks into milliseconds; nullopt when a stamp is missing or
// a stage ends before it starts.
[[nodiscard]] std::optional<FrameGpuTimes> frameGpuTimes(const FrameTimestamps& timestamps,
                                                          std::uint64_t frequency);

// D3D12 timestamp queries for the frames in flight: each back buffer gets its
// own slots, read back once that buffer's fence has passed.
class GpuTimer
{
public:
    [[nodiscard]] static std::expected<GpuTimer, std::string> create(
        ID3D12Device& device, ID3D12CommandQueue& queue, UINT bufferCount);

    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;
    GpuTimer(GpuTimer&&) = default;
    GpuTimer& operator=(GpuTimer&&) = default;

    void beginFrame(ID3D12GraphicsCommandList& commandList, UINT bufferIndex);
    void beginStage(ID3D12GraphicsCommandList& commandList, UINT bufferIndex, EffectStage stage);
    void endStage(ID3D12GraphicsCommandList& commandList, UINT bufferIndex, EffectStage stage);
    void endFrame(ID3D12GraphicsCommandList& commandList, UINT bufferIndex);

    // The GPU times of the last frame recorded into this buffer, once; the
    // data is only valid after the fence that frame signaled.
    [[nodiscard]] std::optional<FrameGpuTimes> take(UINT bufferIndex);

private:
    GpuTimer() = default;

    [[nodiscard]] UINT slot(UINT bufferIndex, std::size_t offset) const;
    void stamp(ID3D12GraphicsCommandList& commandList, UINT bufferIndex, std::size_t offset);

    ComPtr<ID3D12QueryHeap> queryHeap_;
    ComPtr<ID3D12Resource> readback_;
    UINT bufferCount_ = 0;
    std::uint64_t frequency_ = 0;
    std::vector<bool> recorded_;
};

} // namespace lynx::render
