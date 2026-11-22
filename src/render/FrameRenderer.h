#pragma once

#include "core/RateCounter.h"
#include "core/StreamMetrics.h"
#include "core/Windows.h"
#include "render/CadenceDetector.h"
#include "render/CadenceMetric.h"
#include "render/FrameMailbox.h"
#include "render/FramePresenter.h"
#include "render/GpuDevice.h"
#include "render/GpuTimer.h"
#include "render/OpticalFlow.h"
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
        // Computes the driver's optical flow between the picture and the one
        // before it; cadence detection runs either way.
        bool opticalFlow = true;
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

    // Safe from any thread; optical flow starts or stops on the next frame.
    void setOpticalFlow(bool enabled);

    // Safe from any thread; the overlay opens or closes on the next frame.
    void toggleOverlayMenu();
    void toggleOverlayStatistics();
    [[nodiscard]] bool overlayMenuOpen() const;

    // Feeds Dear ImGui's Win32 backend from the window's message loop. The
    // overlay may not exist yet.
    bool handleOverlayMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

private:
    // What a frame records besides the picture: the cadence comparison and,
    // for the video processor's path, the intermediate optical flow reads.
    struct FramePlan
    {
        bool cadence = false;
        bool writeFlowPicture = false;
        bool opticalFlow = false;
        const VideoFrame* previous = nullptr;
    };

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
    void applyRequestedVideoSuperResolution();
    void applyRequestedOpticalFlow();
    // Configures the video processor for the picture on screen and the
    // window it fits; false means the shader's path draws this frame.
    [[nodiscard]] bool prepareVideoSuperResolution();
    // Builds the optical flow session for the picture on screen; false leaves
    // the frame on the block-difference cadence metric alone.
    [[nodiscard]] bool prepareOpticalFlow();
    void recordShaderFrame(UINT bufferIndex, const FramePlan& plan);
    // Copies the frame into the VSR input, runs the processor and records the
    // draw; false when the frame was dropped and the shader path retakes it.
    [[nodiscard]] std::expected<bool, std::string> recordVideoSuperResolutionFrame(
        UINT bufferIndex, const FramePlan& plan);
    void recordInputCopy(ID3D12GraphicsCommandList& commandList, UINT bufferIndex);
    // Signals the cadence fence after a list that recorded a sample.
    void signalCadence();
    // Reads the metric samples whose fences have passed into the detector.
    void drainCadence();
    // Ends the frame after the flow engine's fence, so the stage's time is
    // the engine's real cost.
    [[nodiscard]] std::expected<void, std::string> recordFlowWatch(UINT bufferIndex,
                                                                   bool waitForFlow,
                                                                   std::uint64_t outputValue);
    [[nodiscard]] FlowOverlayState opticalFlowState() const;
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
    std::atomic<bool> requestedVideoSuperResolution_;
    bool requestedVideoSuperResolutionApplied_ = false;
    // Built the first time the setting is on with a picture that can use it;
    // a failure falls back to the shader until the setting changes.
    std::optional<VideoSuperResolution> videoSuperResolution_;
    bool videoSuperResolutionFailed_ = false;
    std::atomic<bool> requestedOpticalFlow_;
    bool requestedOpticalFlowApplied_ = false;
    // Built the first time the setting is on with a picture; a failure keeps
    // cadence detection running on the blocks alone.
    std::optional<OpticalFlow> opticalFlow_;
    bool opticalFlowFailed_ = false;
    // The previous picture's intermediate is usable as flow's reference.
    bool flowReferenceValid_ = false;
    // The block metric is optional: a failure only loses the cadence line.
    std::unique_ptr<CadenceMetric> cadenceMetric_;
    CadenceDetector cadence_;
    // The previous presented picture can be compared against the next one.
    bool cadenceReferenceValid_ = false;
    // A format change restarts the windows like a source change does.
    std::optional<PixelFormat> lastPictureFormat_;
    std::uint64_t cadenceSignal_ = 0;
    std::uint64_t droppedFrames_ = 0;
    // The Reflex frame id of a source frame taken but not presented yet; it
    // survives a frame-slot timeout so the retry reuses its one sleep.
    std::optional<std::uint64_t> reflexFrameId_;
    std::shared_ptr<FrameMailbox> mailbox_;
    Options options_;
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
    // The small list that waits for the flow engine and closes the frame's
    // stamps after it, on its own allocators.
    ComPtr<ID3D12GraphicsCommandList> flowCommandList_;
    std::array<ComPtr<ID3D12CommandAllocator>, SwapChain::kBufferCount> flowAllocators_;
    std::array<UINT64, SwapChain::kBufferCount> flowAllocatorFenceValues_{};
    // Whether the frame each back buffer last showed was a new source frame;
    // the statistics report the video pipeline, not an overlay redraw.
    std::array<bool, SwapChain::kBufferCount> bufferShowedNewFrame_{};
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
