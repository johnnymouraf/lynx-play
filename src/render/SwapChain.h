#pragma once

#include "core/Windows.h"
#include "render/GpuDevice.h"
#include "render/Layout.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace mfplay::render
{

enum class PresentTiming
{
    // Waits for the display's refresh.
    VSync,
    // Shows the frame as soon as it is ready, tearing when the display allows.
    Immediate,
};

// A flip-model swap chain that holds few queued frames (one by default): the
// renderer waits on it before drawing, so every frame shows the newest content.
class SwapChain
{
public:
    static constexpr UINT kBufferCount = 3;
    static constexpr DXGI_FORMAT kSdrFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    static constexpr DXGI_FORMAT kHdrFormat = DXGI_FORMAT_R10G10B10A2_UNORM;

    [[nodiscard]] static std::expected<SwapChain, std::string> create(
        const GpuDevice& gpu, HWND window, SurfaceSize size,
        DXGI_FORMAT format = kSdrFormat);

    // The GPU must be idle: every back buffer is recreated.
    [[nodiscard]] std::expected<void, std::string> resize(SurfaceSize size);

    // Same, with a new back buffer format. A format change recreates the swap
    // chain; an HDR format gets the HDR10 color space.
    [[nodiscard]] std::expected<void, std::string> resize(SurfaceSize size, DXGI_FORMAT format);

    // Blocks until DXGI can take a new frame; false when the timeout expires.
    [[nodiscard]] bool waitForFrameSlot(std::chrono::milliseconds timeout) const;

    // How many frames may wait for the display before the frame slot blocks.
    // One shows the newest content; two lets a source frame's two presents
    // (Frame Generation) queue back to back. The buffer count covers both.
    [[nodiscard]] std::expected<void, std::string> setFrameLatency(UINT frames);
    [[nodiscard]] UINT frameLatency() const;

    [[nodiscard]] HRESULT present(PresentTiming timing);

    [[nodiscard]] UINT currentIndex() const;
    [[nodiscard]] ID3D12Resource* backBuffer(UINT index) const;
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE renderTargetView(UINT index) const;
    [[nodiscard]] SurfaceSize size() const;
    [[nodiscard]] DXGI_FORMAT format() const;
    // The current refresh rate of the monitor that shows the window, in whole
    // hertz; absent when Windows reports none.
    [[nodiscard]] std::optional<double> displayRefreshHertz() const;

private:
    SwapChain() = default;

    [[nodiscard]] std::expected<void, std::string> createChain(SurfaceSize size,
                                                               DXGI_FORMAT format);
    [[nodiscard]] std::expected<void, std::string> applyColorSpace();
    [[nodiscard]] std::expected<void, std::string> createRenderTargets();
    [[nodiscard]] UINT flags() const;

    ComPtr<ID3D12Device> device_;
    ComPtr<IDXGIFactory6> factory_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<IDXGISwapChain3> swapChain_;
    ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    std::array<ComPtr<ID3D12Resource>, kBufferCount> buffers_;
    UINT rtvDescriptorSize_ = 0;
    core::UniqueHandle frameLatencyWaitable_;
    HWND window_ = nullptr;
    SurfaceSize size_;
    DXGI_FORMAT format_ = kSdrFormat;
    UINT frameLatency_ = 1;
    bool tearing_ = false;
};

} // namespace mfplay::render
