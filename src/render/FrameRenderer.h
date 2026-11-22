#pragma once

#include "core/RateCounter.h"
#include "core/Settings.h"
#include "core/StreamMetrics.h"
#include "core/Windows.h"
#include "render/FrameMailbox.h"
#include "render/FramePresenter.h"
#include "render/GpuDevice.h"
#include "render/GpuTimer.h"
#include "render/Hdr.h"
#include "render/Overlay.h"
#include "render/ReflexSession.h"
#include "render/SwapChain.h"
#include "render/VideoFrame.h"
#include "render/VideoSuperResolution.h"

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
        // Runs the stream through RTX Video Super Resolution when the GPU
        // and the driver support it, instead of the shader's scaling pass.
        bool videoSuperResolution = false;
        // What to ask of HDR and what to do with the stream's PQ pictures.
        core::HdrMode hdr = core::HdrMode::Auto;
        // Whether the driver accepted RTX Video HDR's extension.
        bool rtxVideoHdrAvailable = false;
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

    // Safe from any thread; the pipeline switches between the video
    // processor and the shader's path before the next frame.
    void setVideoSuperResolution(bool enabled);

    // Safe from any thread; the HDR path switches before the next frame,
    // recreating the swap chain when the display can show HDR.
    void setHdrMode(core::HdrMode mode);

    // Safe from any thread; the overlay opens or closes on the next frame.
    void toggleOverlayMenu();
    void toggleOverlayStatistics();
    [[nodiscard]] bool overlayMenuOpen() const;

    // Feeds Dear ImGui's Win32 backend from the window's message loop. The
    // overlay may not exist yet.
    bool handleOverlayMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

private:
    // What the pipeline does with the frame on screen.
    struct Presentation
    {
        DXGI_FORMAT targetFormat = SwapChain::kSdrFormat;
        DXGI_FORMAT intermediateFormat = SwapChain::kSdrFormat;
        bool hdrSwapChain = false;
        bool toneMap = false;
        bool rtxVideoHdr = false;
        bool hdrStream = false;
        float contentPeakNits = 1000.0F;
        float sdrWhiteNits = 200.0F;
    };

    FrameRenderer(GpuDevice gpu, HWND window, SwapChain swapChain, FramePresenter presenter,
                  HdrDisplayState display, std::shared_ptr<FrameMailbox> mailbox,
                  Options options);

    [[nodiscard]] std::expected<void, std::string> createFrameResources();
    void run(const std::stop_token& stop);
    [[nodiscard]] Presentation presentationFor(const VideoFrame* frame) const;
    [[nodiscard]] std::expected<void, std::string> applyPresentation(
        const Presentation& presentation);
    void logPresentation(const Presentation& presentation) const;
    void applyOverlayToggles();
    void pollGamepad();
    [[nodiscard]] OverlayGamepad gamepadState() const;
    [[nodiscard]] std::expected<void, std::string> renderFrame();
    [[nodiscard]] std::expected<void, std::string> applyRequestedSize();
    void applyRequestedReflex();
    void applyRequestedVideoSuperResolution();
    void applyRequestedHdr();
    // Configures the video processor for the picture on screen and the
    // window it fits; false means the shader's path draws this frame.
    [[nodiscard]] bool prepareVideoSuperResolution();
    void recordShaderFrame(UINT bufferIndex);
    // Copies the frame into the VSR input, runs the processor and records the
    // draw; false when the frame was dropped and the shader path retakes it.
    [[nodiscard]] std::expected<bool, std::string> recordVideoSuperResolutionFrame(
        UINT bufferIndex);
    void recordInputCopy(ID3D12GraphicsCommandList& commandList, UINT bufferIndex);
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
    HdrDisplayState display_;
    Presentation presentation_;
    bool presentationLogged_ = false;
    std::unique_ptr<Overlay> overlay_;
    std::atomic<Overlay*> overlayForMessages_{nullptr};
    std::optional<GpuTimer> timer_;
    std::optional<FrameGpuTimes> lastGpuTimes_;
    ReflexSession reflex_;
    ReflexUnavailable reflexReason_ = ReflexUnavailable::NoNvidiaGpu;
    std::atomic<core::ReflexMode> requestedReflex_;
    core::ReflexMode requestedReflexApplied_ = core::ReflexMode::Off;
    std::atomic<bool> requestedVideoSuperResolution_;
    bool requestedVideoSuperResolutionApplied_ = false;
    // The processor model that the current frame was prepared with, for the
    // stage stamps.
    bool preparedSuperResolution_ = false;
    bool preparedTrueHdr_ = false;
    // Built the first time the setting is on with a picture that can use it;
    // a failure falls back to the shader until the setting changes.
    std::optional<VideoSuperResolution> videoSuperResolution_;
    bool videoSuperResolutionFailed_ = false;
    std::atomic<core::HdrMode> requestedHdr_;
    core::HdrMode requestedHdrApplied_ = core::HdrMode::Auto;
    // Set when the driver accepted the extension but failed to run it; the
    // pipeline then presents the SDR shader path.
    bool rtxVideoHdrFailed_ = false;
    // The Reflex frame id of a source frame taken but not presented yet; it
    // survives a frame-slot timeout so the retry reuses its one sleep.
    std::optional<std::uint64_t> reflexFrameId_;
    std::shared_ptr<FrameMailbox> mailbox_;
    Options options_;
    OverlayActions overlayActions_;
    std::shared_ptr<core::StreamMetrics> metrics_;
    std::optional<std::chrono::steady_clock::time_point> lastNewFramePresentTime_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    std::array<ComPtr<ID3D12CommandAllocator>, SwapChain::kBufferCount> allocators_;
    std::array<UINT64, SwapChain::kBufferCount> allocatorFenceValues_{};
    // The VSR path's input copy is its own submission, with its own list and
    // allocators, so its fence values are tracked apart from the draw's.
    ComPtr<ID3D12GraphicsCommandList> copyCommandList_;
    std::array<ComPtr<ID3D12CommandAllocator>, SwapChain::kBufferCount> copyAllocators_;
    std::array<UINT64, SwapChain::kBufferCount> copyAllocatorFenceValues_{};
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
