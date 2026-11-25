#pragma once

#include "core/RateCounter.h"
#include "core/Settings.h"
#include "core/StreamMetrics.h"
#include "core/Windows.h"
#include "render/CadenceDetector.h"
#include "render/CadenceMetric.h"
#include "render/DlssSuperResolution.h"
#include "render/FrameGeneration.h"
#include "render/FrameMailbox.h"
#include "render/FramePresenter.h"
#include "render/FrameSchedule.h"
#include "render/GpuDevice.h"
#include "render/GpuTimer.h"
#include "render/Hdr.h"
#include "render/MotionVectors.h"
#include "render/NgxHost.h"
#include "render/OpticalFlow.h"
#include "render/OpticalFlowTimer.h"
#include "render/Overlay.h"
#include "render/PresentGate.h"
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

namespace mfplay::render
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
        // How the stream is scaled to the window: the shader's pass, RTX
        // Video Super Resolution or DLSS Super Resolution.
        core::Upscaling upscaling = core::Upscaling::Shader;
        // Computes the driver's optical flow between the picture and the one
        // before it; cadence detection runs either way, DLSS needs it.
        bool opticalFlow = true;
        // The rate the source delivers frames at, as negotiated; the cadence
        // detection and the frame schedule measure against it.
        double streamFramesPerSecond = 60.0;
        // DLSS Frame Generation; it needs the optical flow and Reflex.
        core::FrameGenerationMode frameGeneration = core::FrameGenerationMode::Off;
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
    // processor, DLSS and the shader's path before the next frame.
    void setUpscaling(core::Upscaling upscaling);

    // Safe from any thread; optical flow starts or stops on the next frame.
    void setOpticalFlow(bool enabled);

    // Safe from any thread; DLSS Frame Generation starts or stops on the next
    // frame.
    void setFrameGeneration(core::FrameGenerationMode mode);

    // Safe from any thread; the HDR path switches before the next frame,
    // recreating the swap chain when the display can show HDR.
    void setHdrMode(core::HdrMode mode);

    // Safe from any thread; the quick menu's window row shows this before
    // the next frame.
    void setWindowLayout(core::WindowMode mode, core::WindowSize size);

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

    // The flow engine's run for this frame. DLSS Super Resolution executes
    // it inside its own submissions, so the renderer must know not to run it
    // again before Frame Generation reads the vectors.
    struct FrameFlow
    {
        bool consumed = false;
        bool submitted = false;
        std::uint64_t output = 0;
    };

    // What the pipeline does with the frame on screen.
    struct Presentation
    {
        DXGI_FORMAT targetFormat = SwapChain::kSdrFormat;
        DXGI_FORMAT intermediateFormat = kIntermediateFormat;
        bool hdrSwapChain = false;
        bool toneMap = false;
        bool rtxVideoHdr = false;
        bool hdrStream = false;
        float contentPeakNits = 1000.0F;
        float sdrWhiteNits = 200.0F;
    };

    // Where a frame draws its final picture: the swap chain's back buffer
    // when Frame Generation is off, or the retained real picture it evaluates
    // and copies from when it is on.
    struct FrameTarget
    {
        ID3D12Resource* resource = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE view{};
        D3D12_RESOURCE_STATES before = D3D12_RESOURCE_STATE_PRESENT;
        D3D12_RESOURCE_STATES after = D3D12_RESOURCE_STATE_PRESENT;
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
    void applyRequestedUpscaling();
    void applyRequestedOpticalFlow();
    void applyRequestedFrameGeneration();
    void applyRequestedHdr();
    void applyRequestedWindowLayout();
    // Configures the video processor for the picture on screen and the
    // window it fits, pointing this slot's scaling view at its output; false
    // means the shader's path draws this frame.
    [[nodiscard]] bool prepareVideoSuperResolution(UINT bufferIndex);
    // Configures the DLSS feature for the picture on screen; false leaves
    // the frame to VSR or the shader, with the reason in the log.
    [[nodiscard]] bool prepareDlssSuperResolution(UINT bufferIndex, bool flowThisFrame);
    // Logs why DLSS is not drawing the stream, once per reason.
    void noteDlssFallback(std::string reason);
    // Builds the optical flow session for the picture on screen; false leaves
    // the frame on the block-difference cadence metric alone.
    [[nodiscard]] bool prepareOpticalFlow();
    // The process's NGX instance for the stream's device, created the first
    // time either NVIDIA feature needs it.
    [[nodiscard]] std::expected<NgxHost*, std::string> ensureNgxHost();
    // Builds or rebuilds the DLSS-G feature, the retained real picture and
    // the motion-vector picture; false draws the frame straight to the swap
    // chain.
    [[nodiscard]] bool prepareFrameGeneration(UINT bufferIndex);
    void releaseFrameGeneration();
    // Releases the optical flow's session and the pictures it read, on the
    // thread that owns them and after the GPU is done; both go before the
    // presenter's own destruction and the device.
    void releaseOpticalFlow();
    // The draw target for this frame: the swap chain when Frame Generation
    // is off, the retained real picture when it is on.
    [[nodiscard]] FrameTarget frameTarget(UINT bufferIndex, bool offscreen);
    // Draws the overlay onto the back buffer, which must be a render target.
    void recordPresentOverlay(ID3D12GraphicsCommandList& list, UINT bufferIndex);
    // Densifies the flow, evaluates DLSS-G, copies the generated picture
    // into the current back buffer, draws the overlay once over it and presents
    // it. False when the evaluation failed and the real picture is the only
    // one this frame can show.
    [[nodiscard]] std::expected<bool, std::string> presentGenerated(
        UINT motionVectorsSlot, std::uint64_t frameId, std::uint64_t flowOutput,
        const FrameGenerationPlan& plan);
    // Copies the retained real picture into the current back buffer and
    // presents it. `secondPresent` waits for the slot the generated picture
    // just took; the frame's first present uses the slot the render loop
    // already waited for. With a frame latency of two that wait returns at
    // once while the display keeps up.
    [[nodiscard]] std::expected<void, std::string> presentReal(std::uint64_t frameId,
                                                               bool secondPresent);
    // Presents the retained real picture again with the overlay drawn over it,
    // without running the scaling and the flow for a picture already composed.
    [[nodiscard]] std::expected<void, std::string> presentOverlayRedraw();
    // Whether the retained real picture still holds the frame on screen, as
    // composed by the current path, so a redraw can present it as it is.
    [[nodiscard]] bool canRepeatRealPicture() const;
    // How long the render loop waits for a frame before it wakes anyway.
    [[nodiscard]] DWORD waitTimeout() const;
    // Asks for a redraw when the overlay is visible and no present has shown it
    // for a while.
    void requestIdleOverlayRedraw();
    // The mailbox's frame when it has one, else `current`; a `current` frame
    // it replaces is dropped.
    [[nodiscard]] std::optional<VideoFrame> takeNewerFrame(std::optional<VideoFrame> current);
    // A frame the loop replaced before showing it. The stream is continuous,
    // so the pair history stays: the next pair spans the gap.
    void countDroppedFrames(std::uint64_t dropped);
    // Frame Generation queues two presents per source frame, so the swap chain
    // may hold two; otherwise one keeps the latency lowest.
    void applyFrameLatency();
    // Reads the display's refresh rate about once a second and tells the
    // schedule.
    void updateDisplayRefresh();
    // Logs once when the display can't show the generated pictures.
    void logDisplayGuard();
    // Optical flow reads the SDR picture, so a PQ stream is tone-mapped for
    // it even when the display takes PQ.
    [[nodiscard]] ToneMapParameters flowToneMap() const;
    void recordShaderFrame(UINT bufferIndex, const FramePlan& plan, const FrameTarget& target);
    // Copies the frame into the VSR input, runs the processor and records the
    // draw; false when the frame was dropped and the shader path retakes it.
    [[nodiscard]] std::expected<bool, std::string> recordVideoSuperResolutionFrame(
        UINT bufferIndex, const FramePlan& plan, const FrameTarget& target);
    // Converts the picture, runs the flow and evaluates DLSS: two
    // submissions, since the feature needs this frame's vectors. Falls back
    // to the shader's draw of the converted picture when the feature fails.
    [[nodiscard]] std::expected<void, std::string> recordDlssSuperResolutionFrame(
        UINT bufferIndex, const FramePlan& plan, const FrameTarget& target, FrameFlow& flow);
    void recordInputCopy(ID3D12GraphicsCommandList& commandList, UINT bufferIndex);
    // Signals the cadence fence after a list that recorded a sample.
    void signalCadence();
    // Reads the metric samples whose fences have passed into the detector.
    void drainCadence();
    // Classifies one frame and reports its distinct/repeated decision to the
    // statistics.
    [[nodiscard]] CadenceDetector::Decision observeCadence(const CadenceSample& sample);
    // Hands a flow the engine accepted to the flow timer, which measures the
    // engine's execution apart from the direct queue.
    void timeOpticalFlow(UINT bufferIndex, std::uint64_t inputValue, std::uint64_t outputValue);
    [[nodiscard]] StageOverlayState opticalFlowState() const;
    [[nodiscard]] StageOverlayState dlssState() const;
    [[nodiscard]] OverlayStats overlayStats();
    [[nodiscard]] std::uint32_t lastGpuWorkUs() const;
    [[nodiscard]] std::string describePresentFailure(HRESULT result) const;
    void reportStatistics();
    void waitForFence(UINT64 value);
    void waitForGpu();

    GpuDevice gpu_;
    HWND window_ = nullptr;
    SwapChain swapChain_;
    // Keeps the frame path idle while the window can't show a picture.
    PresentGate presentGate_;
    // The client area the last frame saw, for the log.
    std::optional<SurfaceSize> lastClientArea_;
    // The last frame found the window hidden; the first one after it starts
    // fresh picture pairs.
    bool hidden_ = false;
    FramePresenter presenter_;
    HdrDisplayState display_;
    Presentation presentation_;
    bool presentationLogged_ = false;
    std::unique_ptr<Overlay> overlay_;
    std::atomic<Overlay*> overlayForMessages_{nullptr};
    std::optional<GpuTimer> timer_;
    std::optional<FrameGpuTimes> lastGpuTimes_;
    // Measured on the flow timer's own queue; absent when that failed.
    std::optional<OpticalFlowTimer> flowTimer_;
    std::optional<double> lastFlowMilliseconds_;
    ReflexSession reflex_;
    ReflexUnavailable reflexReason_ = ReflexUnavailable::NoNvidiaGpu;
    std::atomic<core::ReflexMode> requestedReflex_;
    core::ReflexMode requestedReflexApplied_ = core::ReflexMode::Off;
    std::atomic<core::Upscaling> requestedUpscaling_;
    core::Upscaling requestedUpscalingApplied_ = core::Upscaling::Shader;
    // The processor model that the current frame was prepared with, for the
    // stage stamps.
    bool preparedSuperResolution_ = false;
    bool preparedTrueHdr_ = false;
    // Built the first time the setting is on with a picture that can use it;
    // a failure falls back to the shader until the setting changes.
    std::optional<VideoSuperResolution> videoSuperResolution_;
    bool videoSuperResolutionFailed_ = false;
    // NGX is initialized with the stream's device the first time either
    // feature is asked for and shut down after both are released; each
    // feature is rebuilt for the sizes it runs at.
    std::optional<NgxHost> ngxHost_;
    std::optional<DlssSuperResolution> dlssSuperResolution_;
    bool dlssFailed_ = false;
    // Whether the last prepared frame goes through DLSS, for the overlay.
    bool dlssActive_ = false;
    // DLSS's history must restart after a cut, a dropped frame, a source
    // change or a new feature.
    bool dlssResetPending_ = true;
    // The last reason DLSS left the stream to VSR, so it is logged once.
    std::string dlssFallback_;
    std::atomic<bool> requestedOpticalFlow_;
    bool requestedOpticalFlowApplied_ = false;
    std::atomic<core::FrameGenerationMode> requestedFrameGeneration_;
    core::FrameGenerationMode requestedFrameGenerationApplied_ = core::FrameGenerationMode::Off;
    // The features and the cadence flag only; the GPU objects are built on
    // the first frame that can use them.
    FrameGenerationAvailability frameGenerationAvailability_;
    std::optional<FrameGeneration> frameGeneration_;
    std::optional<MotionVectors> motionVectors_;
    FrameSchedule frameSchedule_;
    bool frameGenerationFailed_ = false;
    std::string frameGenerationFailure_;
    // The composed picture DLSS-G reads and the copy that reaches the screen.
    ComPtr<ID3D12Resource> realPicture_;
    ComPtr<ID3D12DescriptorHeap> realPictureViews_;
    D3D12_CPU_DESCRIPTOR_HANDLE realPictureView_{};
    D3D12_RESOURCE_STATES realPictureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    // The lists that densify, evaluate and copy the two pictures, with their
    // own allocators since a source frame presents twice.
    ComPtr<ID3D12GraphicsCommandList> generationCommandList_;
    std::array<ComPtr<ID3D12CommandAllocator>, SwapChain::kBufferCount> generationAllocators_;
    std::array<UINT64, SwapChain::kBufferCount> generationAllocatorFenceValues_{};
    std::array<ComPtr<ID3D12CommandAllocator>, SwapChain::kBufferCount> presentAllocators_;
    std::array<UINT64, SwapChain::kBufferCount> presentAllocatorFenceValues_{};
    std::uint64_t generatedFrames_ = 0;
    // Built the first time the setting is on with a picture; a failure keeps
    // cadence detection running on the blocks alone.
    std::optional<OpticalFlow> opticalFlow_;
    bool opticalFlowFailed_ = false;
    // The previous picture's intermediate is usable as flow's reference.
    bool flowReferenceValid_ = false;
    // Whether each back buffer's flow texture holds vectors for its picture;
    // DLSS reads them again on a redraw of the same frame.
    std::array<bool, SwapChain::kBufferCount> flowSlotValid_{};
    // The block metric is optional: a failure only loses the cadence line.
    std::unique_ptr<CadenceMetric> cadenceMetric_;
    CadenceDetector cadence_;
    // The previous presented picture can be compared against the next one.
    bool cadenceReferenceValid_ = false;
    // A format change restarts the windows like a source change does.
    std::optional<PixelFormat> lastPictureFormat_;
    std::uint64_t cadenceSignal_ = 0;
    // The value the last recorded frame signaled on the cadence fence, for
    // Frame Generation's one-frame wait for the decision.
    std::uint64_t cadenceFrameValue_ = 0;
    std::uint64_t droppedFrames_ = 0;
    std::atomic<core::HdrMode> requestedHdr_;
    core::HdrMode requestedHdrApplied_ = core::HdrMode::Auto;
    // Read on every present, so the quick menu's toggle applies at once.
    std::atomic<PresentTiming> requestedTiming_;
    // The layout the overlay's window row shows.
    std::atomic<core::WindowMode> requestedWindowMode_;
    std::atomic<core::WindowSize> requestedWindowSize_;
    core::WindowLayout windowLayoutApplied_;
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
    // The source ring advances once per decoded picture, independently of the
    // presents: with Frame Generation a source frame can present twice, so
    // the swap chain's current index no longer marks the previous source.
    UINT sourceSlot_ = 0;
    // Whether the frame each back buffer last showed was a new source frame;
    // the statistics report the video pipeline, not an overlay redraw.
    std::array<bool, SwapChain::kBufferCount> bufferShowedNewFrame_{};
    // The frame each back buffer's last commands read, kept until the GPU is
    // done with them.
    std::array<std::optional<VideoFrame>, SwapChain::kBufferCount> framesInFlight_;
    std::optional<VideoFrame> shownFrame_;
    // A frame taken from the mailbox that waits for the next pass: the window
    // can't show it yet or the swap chain had no free slot.
    std::optional<VideoFrame> pendingFrame_;
    // The real picture Frame Generation holds for a repeated source's repeat.
    HeldRealPicture heldReal_;
    // The retained real picture holds shownFrame_ as the current path
    // composes it.
    bool realPictureCurrent_ = false;
    std::optional<std::chrono::steady_clock::time_point> lastPresentTime_;
    std::optional<double> displayRefreshHertz_;
    std::optional<std::chrono::steady_clock::time_point> displayCheckedAt_;
    bool displayGuardLogged_ = false;
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

} // namespace mfplay::render
