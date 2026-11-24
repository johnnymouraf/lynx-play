#pragma once

#include "core/Windows.h"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

#include <d3d12.h>
#include <wrl/client.h>

namespace mfplay::render
{

using Microsoft::WRL::ComPtr;

// Times the optical flow engine's own execution. The engine runs outside the
// renderer's direct queue, so a stamp there would also count whatever the queue
// does before it reaches the stamp. This timer owns a compute queue that does
// nothing but wait for the engine's fence points and stamp right after each:
// the first stamp is taken when the picture is ready for the engine, the
// second when the engine has written its vectors, both on one queue and clock.
class OpticalFlowTimer
{
public:
    [[nodiscard]] static std::expected<OpticalFlowTimer, std::string> create(
        ID3D12Device& device, UINT slotCount);

    OpticalFlowTimer(const OpticalFlowTimer&) = delete;
    OpticalFlowTimer& operator=(const OpticalFlowTimer&) = delete;
    OpticalFlowTimer(OpticalFlowTimer&&) = default;
    OpticalFlowTimer& operator=(OpticalFlowTimer&&) = default;
    ~OpticalFlowTimer();

    // Queues the two stamps of a flow that execute() accepted: the input and
    // output values are the fence points the flow was started with. Never call
    // it for a flow the engine refused, as the output would never signal.
    [[nodiscard]] std::expected<void, std::string> submit(UINT slot, ID3D12Fence& flowFence,
                                                          std::uint64_t inputValue,
                                                          std::uint64_t outputValue);

    // The engine's time for the last flow submitted for this slot, in
    // milliseconds, once; nullopt while the stamps are pending.
    [[nodiscard]] std::optional<double> take(UINT slot);

    // Blocks until every submitted flow has been stamped.
    void drain();

private:
    struct Slot
    {
        std::uint64_t fenceValue = 0;
        bool pending = false;
    };

    OpticalFlowTimer() = default;

    [[nodiscard]] std::expected<void, std::string> recordStamp(UINT slot, UINT query,
                                                               bool resolve);

    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<ID3D12Fence> fence_;
    ComPtr<ID3D12QueryHeap> queryHeap_;
    ComPtr<ID3D12Resource> readback_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    std::vector<ComPtr<ID3D12CommandAllocator>> allocators_;
    std::vector<Slot> slots_;
    std::uint64_t frequency_ = 0;
    std::uint64_t lastValue_ = 0;
};

} // namespace mfplay::render
