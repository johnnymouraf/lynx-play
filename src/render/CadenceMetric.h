#pragma once

#include "core/Windows.h"
#include "render/VideoFrame.h"

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

#include <d3d12.h>
#include <wrl/client.h>

namespace lynx::render
{

// The 16 bytes the cadence compute shader writes back for one frame.
struct CadenceReadback
{
    std::uint32_t changedBlocks = 0;
    std::uint32_t totalBlocks = 0;
    std::uint32_t sad = 0;
    std::uint32_t reserved = 0;
};

// The block-difference metric behind cadence detection: one compute thread
// per 4x4 luma block, a group reduction and one atomic per group into a
// 16-byte buffer, copied to a readback buffer that the renderer maps on a
// later frame once the fence has passed.
class CadenceMetric
{
public:
    static constexpr UINT kSlotCount = 4;
    static constexpr std::uint32_t kGridSize = 4;
    static constexpr std::uint32_t kSamplesPerBlock = 16;
    // The band, in 8-bit levels, that a sample may move before its block
    // counts as changed.
    static constexpr float kLevelThreshold = 3.0F;

    [[nodiscard]] static std::expected<CadenceMetric, std::string> create(ID3D12Device& device);

    CadenceMetric(const CadenceMetric&) = delete;
    CadenceMetric& operator=(const CadenceMetric&) = delete;
    CadenceMetric(CadenceMetric&&) = default;
    CadenceMetric& operator=(CadenceMetric&&) = default;
    ~CadenceMetric() = default;

    // Records the comparison for `current` against `previous` into
    // `commandList` and returns the fence value to signal after the list has
    // executed; zero when every slot is still in flight.
    [[nodiscard]] std::uint64_t record(ID3D12GraphicsCommandList& commandList,
                                        const VideoFrame& current, const VideoFrame& previous);

    // Reads a completed slot's sample, never waiting. Call it until it
    // returns nullopt.
    [[nodiscard]] std::optional<CadenceReadback> take();

    // Waits for the sample the frame with this fence value produced. Frame
    // generation needs the current frame's decision before it evaluates, so
    // it pays this one short GPU wait instead of guessing a frame late.
    [[nodiscard]] std::optional<CadenceReadback> wait(std::uint64_t fenceValue);

    [[nodiscard]] ID3D12Fence* fence() const;

private:
    CadenceMetric() = default;

    struct Slot
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> counters;
        Microsoft::WRL::ComPtr<ID3D12Resource> readback;
        // The fence value the frame that filled `readback` signaled; zero
        // while the slot is free.
        std::uint64_t pending = 0;
    };

    [[nodiscard]] std::optional<UINT> freeSlot();
    void writeLumaView(UINT slot, bool current, const VideoFrame& frame);

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> viewHeap_;
    UINT descriptorSize_ = 0;
    std::array<Slot, kSlotCount> slots_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    core::UniqueHandle samplesReadyEvent_;
    std::uint64_t nextFenceValue_ = 0;
};

} // namespace lynx::render
