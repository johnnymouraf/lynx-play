#pragma once

#include "core/RateCounter.h"
#include "core/StreamMetrics.h"
#include "core/Windows.h"
#include "render/FrameMailbox.h"
#include "render/FramePresenter.h"
#include "render/GpuDevice.h"
#include "render/GpuTimer.h"
#include "render/Overlay.h"
#include "render/ReflexSession.h"
#include "render/SwapChain.h"
#include "render/VideoFrame.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace lynx::render
{

// Shows the frames a video source publishes, from a thread of its own: each
// new frame is drawn as soon as the swap chain can take it, and the last one
// is drawn again when the window changes size.
class FrameRenderer
{
public:
    struct Options
    {
        PresentTiming timing = PresentTiming::VSync;
        core::ReflexMode reflex = core::ReflexMode::Off;
        OverlayFonts overlayFonts;
        OverlayOptions overlay;
        OverlayActions overlayActions;
        // The counters the overlay reads; shared with the decoder.
        std::shared_ptr<core::StreamMetrics> metrics;
        // The controller the quick menu navigates with; may be empty.
        std::function<OverlayGamepad()> gamepad;
        // Called once on the renderer's thread when it stops for good (the
        // GPU was removed or reset).
        std::function<void()> onFailure;
    };

    [[nodiscard]] static std::expected<std::unique_ptr<FrameRenderer>, std::string> start(
        const GpuDevice& gpu, HWND window, SurfaceSize size, std::shared_ptr<FrameMailbox> mailbox,
        Options options);

    FrameRenderer(const FrameRenderer&) = delete;
    FrameRenderer& operator=(const FrameRenderer&) = delete;
    FrameRenderer(FrameRenderer&&) = delete;
    FrameRenderer& operator=(FrameRenderer&&) = delete;

    // Stops the thread and waits for the GPU, so the window can go away next.
    ~FrameRenderer();

    // Safe from any thread; applied before the next frame. Zero sizes (a
    // minimized window) are ignored.
    void resize(SurfaceSize size);

    // Safe from any thread; applied before the next frame, without stopping
    // the stream.
    void setReflexMode(core::ReflexMode mode);

    // Safe from any thread; the overlay opens or closes on the next frame.
    void toggleOverlayMenu();
    void toggleOverlayStatistics();
    [[nodiscard]] bool overlayMenuOpen() const;

    // Feeds Dear ImGui's Win32 backend from the window's message loop. The
    // overlay may not exist yet.
    bool handleOverlayMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

private:
    FrameRenderer(GpuDevice gpu, HWND window, SwapChain swapChain, FramePresenter presenter,
                  std::shared_ptr<FrameMailbox> mailbox, Options options);

    [[nodiscard]] std::expected<void, std::string> createFrameResources();
    void run(const std::stop_token& stop);
    void applyOverlayToggles();
    void pollGamepad();
    [[nodiscard]] OverlayGamepad gamepadState() const;
    [[nodiscard]] std::expected<void, std::string> renderFrame();
    [[nodiscard]] std::expected<void, std::string> applyRequestedSize();
    void applyRequestedReflex();
    void recordFrame(UINT bufferIndex);
    [[nodiscard]] OverlayStats overlayStats();
    [[nodiscard]] std::uint32_t lastGpuWorkUs() const;
    [[nodiscard]] std::string describePresentFailure(HRESULT result) const;
    void reportStatistics();
    void waitForFence(UINT64 value);
    void waitForGpu();

    GpuDevice gpu_;
    HWND window_ = nullptr;
    SwapChain swapChain_;
    FramePresenter presenter_;
    std::unique_ptr<Overlay> overlay_;
    std::atomic<Overlay*> overlayForMessages_{nullptr};
    std::optional<GpuTimer> timer_;
    std::optional<FrameGpuTimes> lastGpuTimes_;
    ReflexSession reflex_;
    ReflexUnavailable reflexReason_ = ReflexUnavailable::NoNvidiaGpu;
    std::atomic<core::ReflexMode> requestedReflex_;
    core::ReflexMode requestedReflexApplied_ = core::ReflexMode::Off;
    // The Reflex frame id of a source frame taken but not presented yet; it
    // survives a frame-slot timeout so the retry reuses its one sleep.
    std::optional<std::uint64_t> reflexFrameId_;
    std::shared_ptr<FrameMailbox> mailbox_;
    Options options_;
    std::shared_ptr<core::StreamMetrics> metrics_;
    std::optional<std::chrono::steady_clock::time_point> lastPresentTime_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    std::array<ComPtr<ID3D12CommandAllocator>, SwapChain::kBufferCount> allocators_;
    std::array<UINT64, SwapChain::kBufferCount> allocatorFenceValues_{};
    // The frame each back buffer's last commands read, kept until the GPU is
    // done with them.
    std::array<std::optional<VideoFrame>, SwapChain::kBufferCount> framesInFlight_;
    std::optional<VideoFrame> shownFrame_;
    ComPtr<ID3D12Fence> fence_;
    UINT64 lastSignaledFence_ = 0;
    core::UniqueHandle fenceEvent_;
    core::UniqueHandle wakeEvent_;
    std::atomic<SurfaceSize> requestedSize_;
    std::atomic<bool> redrawRequested_ = false;
    std::atomic<bool> menuToggleRequested_ = false;
    std::atomic<bool> statisticsToggleRequested_ = false;
    bool chordHeld_ = false;
    core::RateCounter presented_;
    // Last, so the thread starts after every member exists and stops first.
    std::jthread thread_;
};

} // namespace lynx::render
