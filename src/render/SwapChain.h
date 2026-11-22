#pragma once

#include "core/Windows.h"
#include "render/GpuDevice.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <string>

namespace lynx::render
{

struct SurfaceSize
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    bool operator==(const SurfaceSize&) const = default;
};

enum class PresentTiming
{
    // Waits for the display's refresh.
    VSync,
    // Shows the frame as soon as it is ready, tearing when the display allows.
    Immediate,
};

// A flip-model swap chain that holds at most one queued frame: the renderer
// waits on it before drawing, so every frame shows the newest content.
class SwapChain
{
public:
    static constexpr UINT kBufferCount = 3;

    [[nodiscard]] static std::expected<SwapChain, std::string> create(const GpuDevice& gpu,
                                                                      HWND window,
                                                                      SurfaceSize size);

    // The GPU must be idle: every back buffer is recreated.
    [[nodiscard]] std::expected<void, std::string> resize(SurfaceSize size);

    // Blocks until DXGI can take a new frame; false when the timeout expires.
    [[nodiscard]] bool waitForFrameSlot(std::chrono::milliseconds timeout) const;

    [[nodiscard]] HRESULT present(PresentTiming timing);

    [[nodiscard]] UINT currentIndex() const;
    [[nodiscard]] ID3D12Resource* backBuffer(UINT index) const;
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView(UINT index) const;
    [[nodiscard]] SurfaceSize size() const;

private:
    SwapChain() = default;

    [[nodiscard]] std::expected<void, std::string> createRenderTargets();
    [[nodiscard]] UINT flags() const;

    ComPtr<ID3D12Device> device_;
    ComPtr<IDXGISwapChain3> swapChain_;
    ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    std::array<ComPtr<ID3D12Resource>, kBufferCount> buffers_;
    UINT rtvDescriptorSize_ = 0;
    core::UniqueHandle frameLatencyWaitable_;
    SurfaceSize size_;
    bool tearing_ = false;
};

} // namespace lynx::render
