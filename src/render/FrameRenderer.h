#pragma once

#include "core/Windows.h"
#include "render/GpuDevice.h"
#include "render/SwapChain.h"

#include <array>
#include <atomic>
#include <chrono>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>

namespace lynx::render
{

// Draws into a window from its own thread, one frame per free swap chain slot.
// For now it draws the test pattern.
class FrameRenderer
{
public:
    [[nodiscard]] static std::expected<std::unique_ptr<FrameRenderer>, std::string> start(
        HWND window, SurfaceSize size);

    FrameRenderer(const FrameRenderer&) = delete;
    FrameRenderer& operator=(const FrameRenderer&) = delete;
    FrameRenderer(FrameRenderer&&) = delete;
    FrameRenderer& operator=(FrameRenderer&&) = delete;

    // Stops the thread and waits for the GPU, so the window can go away next.
    ~FrameRenderer();

    // Safe from any thread; applied before the next frame. Zero sizes (a
    // minimized window) are ignored.
    void resize(SurfaceSize size);

private:
    FrameRenderer(GpuDevice gpu, SwapChain swapChain);

    [[nodiscard]] std::expected<void, std::string> createFrameResources();
    void run(const std::stop_token& stop);
    [[nodiscard]] std::expected<void, std::string> renderFrame();
    [[nodiscard]] std::expected<void, std::string> applyRequestedSize();
    void recordTestPattern(UINT bufferIndex);
    void waitForFence(UINT64 value);
    void waitForGpu();

    GpuDevice gpu_;
    SwapChain swapChain_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    std::array<ComPtr<ID3D12CommandAllocator>, SwapChain::kBufferCount> allocators_;
    std::array<UINT64, SwapChain::kBufferCount> allocatorFenceValues_{};
    ComPtr<ID3D12Fence> fence_;
    UINT64 lastSignaledFence_ = 0;
    core::UniqueHandle fenceEvent_;
    std::atomic<SurfaceSize> requestedSize_;
    std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();
    // Last, so the thread starts after every member exists and stops first.
    std::jthread thread_;
};

} // namespace lynx::render
