#include "render/FrameRenderer.h"

#include "core/Latency.h"
#include "core/TerminateLogging.h"

#include <algorithm>
#include <format>
#include <span>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

namespace mfplay::render
{

namespace
{

using core::describeFailure;
using Clock = std::chrono::steady_clock;

// Long enough to never fire while frames flow; on expiry the frame is tried
// again.
constexpr std::chrono::milliseconds kFrameSlotTimeout{100};
constexpr std::chrono::seconds kStatisticsPeriod{5};

constexpr std::array<float, 4> kBlack{0.0F, 0.0F, 0.0F, 1.0F};

D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;

    return barrier;
}

std::string reflexModeLabel(core::ReflexMode mode)
{
    switch (mode)
    {
    case core::ReflexMode::Off:
        return "Desligado";
    case core::ReflexMode::On:
        return "Ligado";
    case core::ReflexMode::Boost:
        return "Ligado + Boost";
    }

    return "Desligado";
}

std::string frameGenerationUnavailableReason(FrameGenerationUnavailable reason)
{
    switch (reason)
    {
    case FrameGenerationUnavailable::NoNvidiaGpu:
        return "the stream GPU is not NVIDIA";
    case FrameGenerationUnavailable::RuntimeMissing:
        return "the DLSS-G runtime is missing";
    case FrameGenerationUnavailable::HagsOff:
        return "hardware-accelerated GPU scheduling is off";
    case FrameGenerationUnavailable::DriverTooOld:
        return "the NVIDIA driver is too old";
    case FrameGenerationUnavailable::NotAvailable:
        return "the GPU does not support DLSS Frame Generation";
    case FrameGenerationUnavailable::DriverFailed:
        return "the NVIDIA driver refused NGX";
    case FrameGenerationUnavailable::None:
        break;
    }

    return "unavailable";
}

std::string reflexUnavailableReason(ReflexUnavailable reason)
{
    switch (reason)
    {
    case ReflexUnavailable::NoNvidiaGpu:
        return "GPU não NVIDIA";
    case ReflexUnavailable::NoDriver:
        return "driver NVIDIA não encontrado";
    case ReflexUnavailable::DriverFailed:
        return "o driver NVIDIA recusou o NVAPI";
    case ReflexUnavailable::None:
        break;
    }

    return "indisponível";
}

SurfaceSize clientAreaOf(HWND window)
{
    // An iconic window has no picture on screen even if its stored client
    // rectangle still reads the restored size.
    if (IsIconic(window) != FALSE)
    {
        return {};
    }

    RECT client{};
    GetClientRect(window, &client);

    return {
        .width = static_cast<std::uint32_t>(client.right - client.left),
        .height = static_cast<std::uint32_t>(client.bottom - client.top),
    };
}

PresentOutcome outcomeOf(HRESULT result)
{
    return result == DXGI_STATUS_OCCLUDED ? PresentOutcome::Occluded : PresentOutcome::Presented;
}

float contentPeakNitsOf(const VideoFrame& frame)
{
    double peak = frame.hdr.maxContentLightNits;

    if (peak <= 0.0)
    {
        peak = frame.hdr.maxMasteringLuminanceNits;
    }

    if (peak <= 0.0)
    {
        peak = 1000.0;
    }

    return static_cast<float>(std::clamp(peak, 100.0, 10000.0));
}

} // namespace

std::expected<std::unique_ptr<FrameRenderer>, std::string> FrameRenderer::start(
    const GpuDevice& gpu, HWND window, SurfaceSize size, std::shared_ptr<FrameMailbox> mailbox,
    Options options)
{
    auto swapChain = SwapChain::create(gpu, window, size);

    if (!swapChain)
    {
        return std::unexpected(swapChain.error());
    }

    auto presenter = FramePresenter::create(*gpu.device.Get(), SwapChain::kBufferCount,
                                            kIntermediateFormat, SwapChain::kSdrFormat);

    if (!presenter)
    {
        return std::unexpected(presenter.error());
    }

    // The constructor is private, out of make_unique's reach.
    std::unique_ptr<FrameRenderer> renderer(new FrameRenderer(
        gpu, window, std::move(*swapChain), std::move(*presenter), detectHdrDisplay(window),
        std::move(mailbox), std::move(options)));

    if (auto resources = renderer->createFrameResources(); !resources)
    {
        return std::unexpected(resources.error());
    }

    // The first frame is black, until the source sends a picture.
    renderer->redrawRequested_.store(true);
    SetEvent(renderer->wakeEvent_.get());

    FrameRenderer* const self = renderer.get();
    renderer->thread_ = std::jthread([self](const std::stop_token& stop) { self->run(stop); });

    return renderer;
}

FrameRenderer::FrameRenderer(GpuDevice gpu, HWND window, SwapChain swapChain,
                             FramePresenter presenter, HdrDisplayState display,
                             std::shared_ptr<FrameMailbox> mailbox, Options options)
    : gpu_(std::move(gpu)),
      window_(window),
      swapChain_(std::move(swapChain)),
      presenter_(std::move(presenter)),
      display_(std::move(display)),
      mailbox_(std::move(mailbox)),
      options_(std::move(options)),
      metrics_(options_.metrics),
      requestedSize_(swapChain_.size()),
      presented_(std::chrono::steady_clock::now(), kStatisticsPeriod)
{
    requestedReflex_.store(options_.reflex);
    requestedUpscaling_.store(options_.upscaling);
    requestedOpticalFlow_.store(options_.opticalFlow);
    requestedFrameGeneration_.store(options_.frameGeneration);
    requestedHdr_.store(options_.hdr);
    requestedHdrApplied_ = options_.hdr;
    requestedTiming_.store(options_.timing);
    requestedWindowMode_.store(options_.overlay.windowMode);
    requestedWindowSize_.store(options_.overlay.windowSize);
    windowLayoutApplied_ = {
        .mode = options_.overlay.windowMode,
        .size = options_.overlay.windowSize,
    };

    spdlog::info("Display for the stream: '{}', HDR supported {}, HDR active {}, SDR white "
                 "{:.0f} nits (DisplayConfig)",
                 display_.name.empty() ? "unknown" : display_.name,
                 display_.hdrSupported ? "yes" : "no", display_.hdrActive ? "yes" : "no",
                 display_.sdrWhiteNits);

    if (display_.hdrSupported && !display_.hdrActive)
    {
        spdlog::info("HDR display with Windows HDR off; HDR streams are tone-mapped to SDR. Turn "
                     "HDR on in Windows Settings > System > Display > HDR.");
    }

    if (options_.hdr == core::HdrMode::RtxVideoHdr)
    {
        if (!display_.hdrActive)
        {
            spdlog::info("RTX Video HDR: requested, but the display isn't in HDR; the SDR shader "
                         "path draws the stream");
        }
        else if (!options_.rtxVideoHdrAvailable)
        {
            spdlog::info("RTX Video HDR: requested, but the driver extension isn't available; the "
                         "SDR shader path draws the stream");
        }
    }
}

FrameRenderer::~FrameRenderer()
{
    if (thread_.joinable())
    {
        thread_.request_stop();
        thread_.join();
    }

    waitForGpu();
    spdlog::info("The frame renderer's GPU work is complete (fence {}); releasing the features",
                 lastSignaledFence_);

    // The render thread normally releases the flow before it stops; this
    // covers a renderer whose thread stopped early.
    releaseOpticalFlow();
    releaseFrameGeneration();
    // The features must be gone before the host shuts NGX down.
    dlssSuperResolution_.reset();
    ngxHost_.reset();

    reflex_.shutdown();
    spdlog::info("The frame features are released; the session's own objects go next");
}

void FrameRenderer::resize(SurfaceSize size)
{
    if (size.width == 0 || size.height == 0)
    {
        return;
    }

    requestedSize_.store(size);
    redrawRequested_.store(true);
    SetEvent(wakeEvent_.get());
}

void FrameRenderer::setReflexMode(core::ReflexMode mode)
{
    requestedReflex_.store(mode);
    SetEvent(wakeEvent_.get());
}

void FrameRenderer::setUpscaling(core::Upscaling upscaling)
{
    requestedUpscaling_.store(upscaling);
    SetEvent(wakeEvent_.get());
}

void FrameRenderer::setOpticalFlow(bool enabled)
{
    requestedOpticalFlow_.store(enabled);
    SetEvent(wakeEvent_.get());
}

void FrameRenderer::setFrameGeneration(core::FrameGenerationMode mode)
{
    requestedFrameGeneration_.store(mode);
    SetEvent(wakeEvent_.get());
}

void FrameRenderer::setHdrMode(core::HdrMode mode)
{
    requestedHdr_.store(mode);
    SetEvent(wakeEvent_.get());
}

void FrameRenderer::setWindowLayout(core::WindowMode mode, core::WindowSize size)
{
    requestedWindowMode_.store(mode);
    requestedWindowSize_.store(size);
    redrawRequested_.store(true);
    SetEvent(wakeEvent_.get());
}

void FrameRenderer::toggleOverlayMenu()
{
    menuToggleRequested_.store(true);
    redrawRequested_.store(true);
    SetEvent(wakeEvent_.get());
}

void FrameRenderer::toggleOverlayStatistics()
{
    statisticsToggleRequested_.store(true);
    redrawRequested_.store(true);
    SetEvent(wakeEvent_.get());
}

bool FrameRenderer::overlayMenuOpen() const
{
    const Overlay* const overlay = overlayForMessages_.load();
    return overlay != nullptr && overlay->menuOpen();
}

bool FrameRenderer::handleOverlayMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    Overlay* const overlay = overlayForMessages_.load();
    return overlay != nullptr && overlay->handleMessage(window, message, wParam, lParam);
}

std::expected<void, std::string> FrameRenderer::createFrameResources()
{
    for (auto& allocator : allocators_)
    {
        if (const HRESULT result = gpu_.device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
            FAILED(result))
        {
            return std::unexpected(describeFailure("CreateCommandAllocator", result));
        }
    }

    if (const HRESULT result = gpu_.device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators_[0].Get(), nullptr,
            IID_PPV_ARGS(&commandList_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommandList", result));
    }

    commandList_->Close();

    for (auto& allocator : copyAllocators_)
    {
        if (const HRESULT result = gpu_.device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
            FAILED(result))
        {
            return std::unexpected(describeFailure("CreateCommandAllocator (VSR)", result));
        }
    }

    if (const HRESULT result = gpu_.device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, copyAllocators_[0].Get(), nullptr,
            IID_PPV_ARGS(&copyCommandList_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommandList (VSR)", result));
    }

    copyCommandList_->Close();

    for (auto& allocator : generationAllocators_)
    {
        if (const HRESULT result = gpu_.device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
            FAILED(result))
        {
            return std::unexpected(
                describeFailure("CreateCommandAllocator (frame generation)", result));
        }
    }

    for (auto& allocator : presentAllocators_)
    {
        if (const HRESULT result = gpu_.device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
            FAILED(result))
        {
            return std::unexpected(
                describeFailure("CreateCommandAllocator (generated present)", result));
        }
    }

    if (const HRESULT result = gpu_.device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, generationAllocators_[0].Get(), nullptr,
            IID_PPV_ARGS(&generationCommandList_));
        FAILED(result))
    {
        return std::unexpected(
            describeFailure("CreateCommandList (frame generation)", result));
    }

    generationCommandList_->Close();

    if (const HRESULT result =
            gpu_.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateFence", result));
    }

    auto timer = GpuTimer::create(*gpu_.device.Get(), *gpu_.queue.Get(), SwapChain::kBufferCount);

    if (!timer)
    {
        return std::unexpected(timer.error());
    }

    timer_ = std::move(*timer);

    if (auto flowTimer = OpticalFlowTimer::create(*gpu_.device.Get(), SwapChain::kBufferCount);
        flowTimer)
    {
        flowTimer_ = std::move(*flowTimer);
    }
    else
    {
        spdlog::warn("The optical flow's own time won't be measured: {}", flowTimer.error());
    }

    fenceEvent_.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    wakeEvent_.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));

    if (!fenceEvent_ || !wakeEvent_)
    {
        return std::unexpected(describeFailure("CreateEventW", HRESULT_FROM_WIN32(GetLastError())));
    }

    return {};
}

void FrameRenderer::run(const std::stop_token& stop)
try
{
    SetThreadDescription(GetCurrentThread(), L"MFPlay frame renderer");
    const core::MmcssRegistration mmcss = core::MmcssRegistration::enable(L"Games");

    if (gpu_.dxgiDevice)
    {
        raiseCurrentThreadGpuPriority(*gpu_.dxgiDevice.Get());
    }

    const ReflexAvailability availability = ReflexSession::probe();
    reflexReason_ = availability.reason;

    if (!availability.available)
    {
        spdlog::info("reflex: unavailable ({})", availability.detail);
    }
    else if (!reflex_.open())
    {
        reflexReason_ = ReflexUnavailable::DriverFailed;
        spdlog::warn("reflex: {}", reflex_.detail());
    }

    requestedReflexApplied_ = requestedReflex_.load();
    reflex_.configure(requestedReflexApplied_, gpu_.device.Get());
    requestedOpticalFlowApplied_ = requestedOpticalFlow_.load();
    requestedUpscalingApplied_ = requestedUpscaling_.load();
    spdlog::info("Upscaling: {}", core::upscalingName(requestedUpscalingApplied_));

    frameGenerationAvailability_ = FrameGeneration::probe();

    if (frameGenerationAvailability_.available)
    {
        spdlog::info("Frame Generation is available: {}", frameGenerationAvailability_.detail);
    }
    else
    {
        spdlog::info("Frame Generation is unavailable: {} ({})",
                     frameGenerationUnavailableReason(frameGenerationAvailability_.reason),
                     frameGenerationAvailability_.detail);
    }

    if (auto metric = CadenceMetric::create(*gpu_.device.Get()); metric)
    {
        cadenceMetric_ = std::make_unique<CadenceMetric>(std::move(*metric));
    }
    else
    {
        spdlog::warn("Cadence detection is unavailable: {}", metric.error());
    }

    spdlog::info("Optical flow: {}", requestedOpticalFlowApplied_ ? "on" : "off");

    overlayActions_ = options_.overlayActions;
    const std::function<void(core::ReflexMode)> appReflex = overlayActions_.reflexChanged;

    overlayActions_.reflexChanged = [this, appReflex](core::ReflexMode mode)
    {
        requestedReflex_.store(mode);
        SetEvent(wakeEvent_.get());

        if (appReflex)
        {
            appReflex(mode);
        }
    };

    const std::function<void(core::Upscaling)> appUpscaling = overlayActions_.upscalingChanged;

    overlayActions_.upscalingChanged = [this, appUpscaling](core::Upscaling upscaling)
    {
        requestedUpscaling_.store(upscaling);
        SetEvent(wakeEvent_.get());

        if (appUpscaling)
        {
            appUpscaling(upscaling);
        }
    };

    const std::function<void(core::FrameGenerationMode)> appFrameGeneration =
        overlayActions_.frameGenerationChanged;

    overlayActions_.frameGenerationChanged = [this, appFrameGeneration](
                                                 core::FrameGenerationMode mode)
    {
        requestedFrameGeneration_.store(mode);
        SetEvent(wakeEvent_.get());

        if (appFrameGeneration)
        {
            appFrameGeneration(mode);
        }
    };

    const std::function<void(bool)> appLowestLatency = overlayActions_.lowestLatencyChanged;

    overlayActions_.lowestLatencyChanged = [this, appLowestLatency](bool lowest)
    {
        requestedTiming_.store(lowest ? PresentTiming::Immediate : PresentTiming::VSync);
        SetEvent(wakeEvent_.get());

        if (appLowestLatency)
        {
            appLowestLatency(lowest);
        }
    };

    OverlayOptions overlayOptions = options_.overlay;
    overlayOptions.hdrOutput = presentation_.hdrSwapChain;
    overlayOptions.sdrWhiteNits = presentation_.sdrWhiteNits;

    auto created = Overlay::create(*gpu_.device.Get(), *gpu_.queue.Get(), window_,
                                   SwapChain::kBufferCount, swapChain_.format(),
                                   options_.overlayFonts, overlayOptions, overlayActions_);

    if (!created)
    {
        spdlog::warn("The overlay is unavailable: {}", created.error());
    }
    else
    {
        overlay_ = std::move(*created);
        overlayForMessages_.store(overlay_.get());
    }

    const std::stop_callback wakeOnStop(stop, [this] { SetEvent(wakeEvent_.get()); });
    const std::array events{mailbox_->frameReadyEvent(), wakeEvent_.get()};
    const DWORD timeout = options_.gamepad ? 16 : INFINITE;

    while (true)
    {
        WaitForMultipleObjects(static_cast<DWORD>(events.size()), events.data(), FALSE, timeout);

        if (stop.stop_requested())
        {
            break;
        }

        applyRequestedReflex();
        applyRequestedUpscaling();
        applyRequestedOpticalFlow();
        applyRequestedFrameGeneration();
        applyRequestedHdr();
        applyRequestedWindowLayout();
        applyOverlayToggles();
        pollGamepad();

        if (auto frame = renderFrame(); !frame)
        {
            spdlog::error("Rendering stopped: {}", frame.error());

            if (options_.onFailure)
            {
                options_.onFailure();
            }

            break;
        }

        if (overlay_ != nullptr && overlay_->visible())
        {
            redrawRequested_.store(true);
            SetEvent(wakeEvent_.get());
        }

        reportStatistics();
    }

    overlayForMessages_.store(nullptr);

    // The overlay's buffers and the frames in flight are released below, so
    // the GPU must be done with them first.
    waitForGpu();
    // The flow's session and pictures belong to this thread; release them
    // here, while the presenter and the device that own them are still alive.
    releaseOpticalFlow();
}
catch (...)
{
    core::logUnhandledException("The render thread stopped on an unhandled exception");
    throw;
}

void FrameRenderer::applyOverlayToggles()
{
    if (overlay_ == nullptr)
    {
        return;
    }

    if (menuToggleRequested_.exchange(false))
    {
        overlay_->toggleMenu();
        redrawRequested_.store(true);
    }

    if (statisticsToggleRequested_.exchange(false))
    {
        overlay_->toggleStatistics();
        redrawRequested_.store(true);
    }
}

OverlayGamepad FrameRenderer::gamepadState() const
{
    return options_.gamepad ? options_.gamepad() : OverlayGamepad{};
}

void FrameRenderer::pollGamepad()
{
    if (overlay_ == nullptr)
    {
        return;
    }

    const OverlayGamepad gamepad = gamepadState();
    const bool chord = gamepad.connected && gamepad.create && gamepad.options;

    if (chord && !chordHeld_)
    {
        overlay_->toggleMenu();
        redrawRequested_.store(true);
    }

    chordHeld_ = chord;
}

std::expected<void, std::string> FrameRenderer::renderFrame()
{
    drainCadence();

    // A minimized window has an empty client area: presenting into it would
    // run the whole frame for a picture nobody sees, so the pipeline stays
    // idle until it comes back. The newest picture waits for the redraw.
    const SurfaceSize client = clientAreaOf(window_);

    if (!lastClientArea_ || *lastClientArea_ != client)
    {
        spdlog::debug("Stream window client area: {}x{}", client.width, client.height);
        lastClientArea_ = client;
    }

    presentGate_.setClientSize(client);

    std::optional<VideoFrame> newest = mailbox_->take();
    const bool redraw = redrawRequested_.exchange(false);

    if (!presentGate_.mayPresent())
    {
        if (!hidden_)
        {
            spdlog::debug("The stream window can't show a picture; the frame path is idle");
        }

        hidden_ = true;

        if (newest)
        {
            shownFrame_ = std::move(newest);
        }

        redrawRequested_.store(true);
        return {};
    }

    if (hidden_)
    {
        // The first picture after the window comes back has no pair with the
        // one from before it: DLSS-G and the cadence start over.
        hidden_ = false;
        cadenceReferenceValid_ = false;
        flowReferenceValid_ = false;
        dlssResetPending_ = true;
        frameSchedule_.reset();
        spdlog::debug("The stream window shows a picture again; the frame path resumes");
    }

    if (!newest && !redraw)
    {
        return {};
    }

    // The first new frame of this pass is accepted here, so its one sleep
    // comes before any of its work; a retry reuses the same id.
    if (newest && !reflexFrameId_)
    {
        reflexFrameId_ = reflex_.beginSourceFrame(lastGpuWorkUs());
    }

    // Each wait takes a slot that only a present gives back, so it comes
    // after deciding to present.
    if (!swapChain_.waitForFrameSlot(kFrameSlotTimeout))
    {
        if (newest)
        {
            shownFrame_ = std::move(newest);
        }

        redrawRequested_.store(true);
        SetEvent(wakeEvent_.get());
        return {};
    }

    // A frame that arrived during the wait is newer still.
    if (auto newer = mailbox_->take())
    {
        newest = std::move(newer);
    }

    const bool showsNewFrame = newest.has_value();

    if (showsNewFrame && !reflexFrameId_)
    {
        reflexFrameId_ = reflex_.beginSourceFrame(lastGpuWorkUs());
    }

    if (showsNewFrame)
    {
        shownFrame_ = std::move(newest);

        // A frame the mailbox replaced breaks the pair sequence: the next
        // picture is no longer the one the metric compares against.
        if (const std::uint64_t dropped = mailbox_->takeDroppedCount(); dropped > 0)
        {
            droppedFrames_ += dropped;
            cadence_.reset();
            cadenceReferenceValid_ = false;
            flowReferenceValid_ = false;
            dlssResetPending_ = true;
            frameSchedule_.reset();
        }
    }

    if (auto resized = applyRequestedSize(); !resized)
    {
        return resized;
    }

    if (shownFrame_ && lastPictureFormat_ && *lastPictureFormat_ != shownFrame_->format)
    {
        // A different source format starts a new compare window.
        cadence_.reset();
        cadenceReferenceValid_ = false;
        flowReferenceValid_ = false;
        flowSlotValid_.fill(false);
        dlssResetPending_ = true;
        frameSchedule_.reset();
        frameSchedule_.setCadence(std::nullopt);
    }

    const Presentation presentation = presentationFor(shownFrame_ ? &*shownFrame_ : nullptr);

    if (auto applied = applyPresentation(presentation); !applied)
    {
        return applied;
    }

    if (shownFrame_)
    {
        lastPictureFormat_ = shownFrame_->format;

        const SurfaceSize picture{.width = shownFrame_->width, .height = shownFrame_->height};

        if (presenter_.needsPicture(picture))
        {
            waitForGpu();
            // The old pictures go before the presenter replaces them; the
            // engine's registration goes first.
            releaseOpticalFlow();

            if (auto resized = presenter_.resizePicture(picture); !resized)
            {
                return resized;
            }

            // The new textures have no history: restart both windows.
            cadence_.reset();
            cadenceReferenceValid_ = false;
            flowReferenceValid_ = false;
            flowSlotValid_.fill(false);
            dlssResetPending_ = true;
            frameSchedule_.reset();
            frameSchedule_.setCadence(std::nullopt);
        }
    }

    // The swap chain's current index is the present target; the source ring
    // advances once per decoded picture, since Frame Generation can present
    // twice for the same one.
    const UINT presentBufferIndex = swapChain_.currentIndex();

    if (showsNewFrame)
    {
        sourceSlot_ = (sourceSlot_ + 1) % SwapChain::kBufferCount;
    }

    const UINT sourceIndex = sourceSlot_;

    // A source slot's command allocator can be reset only once the GPU has
    // finished the frame that last used it; so can the picture it read.
    waitForFence(allocatorFenceValues_.at(sourceIndex));
    waitForFence(copyAllocatorFenceValues_.at(sourceIndex));
    framesInFlight_.at(sourceIndex).reset();

    if (auto times = timer_->take(sourceIndex); times && bufferShowedNewFrame_.at(sourceIndex))
    {
        lastGpuTimes_ = *times;
    }

    if (flowTimer_)
    {
        if (const auto flowMilliseconds = flowTimer_->take(sourceIndex))
        {
            lastFlowMilliseconds_ = *flowMilliseconds;
        }
    }

    const bool flowReady = prepareOpticalFlow();
    const bool offscreen = prepareFrameGeneration(sourceIndex);

    const UINT previousIndex =
        (sourceIndex + SwapChain::kBufferCount - 1) % SwapChain::kBufferCount;
    const std::optional<VideoFrame>& previousFrame = framesInFlight_.at(previousIndex);

    FramePlan plan;
    plan.previous = previousFrame ? &*previousFrame : nullptr;
    plan.writeFlowPicture = flowReady && shownFrame_.has_value();
    plan.cadence = showsNewFrame && cadenceMetric_ && cadenceReferenceValid_ &&
                   plan.previous != nullptr &&
                   plan.previous->width == shownFrame_->width &&
                   plan.previous->height == shownFrame_->height &&
                   plan.previous->format == shownFrame_->format;
    plan.opticalFlow = showsNewFrame && flowReady && flowReferenceValid_;
    cadenceSignal_ = 0;
    cadenceFrameValue_ = 0;

    const bool marked = reflexFrameId_.has_value();
    const std::uint64_t reflexFrameId = reflexFrameId_.value_or(0);
    FrameFlow flow;
    dlssActive_ = false;

    if (marked)
    {
        reflex_.mark(reflexFrameId, LatencyMarker::SimulationStart);
        reflex_.mark(reflexFrameId, LatencyMarker::SimulationEnd);
        reflex_.mark(reflexFrameId, LatencyMarker::RenderSubmitStart);
    }

    const FrameTarget target = frameTarget(presentBufferIndex, offscreen);

    if (prepareDlssSuperResolution(sourceIndex, plan.opticalFlow))
    {
        const auto recorded =
            recordDlssSuperResolutionFrame(sourceIndex, plan, target, flow);

        if (!recorded)
        {
            return std::unexpected(recorded.error());
        }
    }
    else if (prepareVideoSuperResolution(sourceIndex))
    {
        const auto recorded = recordVideoSuperResolutionFrame(sourceIndex, plan, target);

        if (!recorded)
        {
            return std::unexpected(recorded.error());
        }

        // The processor failed; the shader path retakes the same frame.
        if (!*recorded)
        {
            return {};
        }
    }
    else
    {
        auto& allocator = allocators_.at(sourceIndex);
        allocator->Reset();
        commandList_->Reset(allocator.Get(), nullptr);
        recordShaderFrame(sourceIndex, plan, target);

        if (const HRESULT result = commandList_->Close(); FAILED(result))
        {
            return std::unexpected(describeFailure("ID3D12GraphicsCommandList::Close", result));
        }

        if (shownFrame_ && shownFrame_->fence)
        {
            gpu_.queue->Wait(shownFrame_->fence.Get(), shownFrame_->fenceValue);
        }

        const std::array<ID3D12CommandList*, 1> lists{commandList_.Get()};
        gpu_.queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());
        signalCadence();
    }

    if (plan.opticalFlow && !flow.consumed)
    {
        const std::uint64_t inputValue = opticalFlow_->nextInputValue();
        gpu_.queue->Signal(opticalFlow_->fence(), inputValue);

        const auto output = opticalFlow_->execute(sourceIndex, inputValue);

        if (output)
        {
            flow.submitted = true;
            flow.output = *output;
            flowSlotValid_.at(sourceIndex) = true;
            timeOpticalFlow(sourceIndex, inputValue, *output);
        }
        else
        {
            opticalFlowFailed_ = true;
            redrawRequested_.store(true);
            SetEvent(wakeEvent_.get());
            spdlog::error("Optical flow stopped; cadence runs on the block metric alone: {}",
                          output.error());
        }
    }

    if (marked)
    {
        reflex_.mark(reflexFrameId, LatencyMarker::RenderSubmitEnd);
    }

    bool realPresented = false;

    if (offscreen)
    {
        FrameGenerationPlan generation;

        if (showsNewFrame)
        {
            CadenceDetector::Decision decision = CadenceDetector::Decision::Ignored;

            if (cadenceFrameValue_ != 0 && cadenceMetric_ != nullptr)
            {
                if (const auto sample = cadenceMetric_->wait(cadenceFrameValue_))
                {
                    const double blockSamples = static_cast<double>(sample->totalBlocks) *
                                                 CadenceMetric::kSamplesPerBlock;
                    const CadenceSample comparison{
                        .changedBlocks = sample->changedBlocks,
                        .totalBlocks = sample->totalBlocks,
                        .meanDifference = blockSamples > 0.0
                                              ? static_cast<double>(sample->sad) / blockSamples
                                              : 0.0,
                    };
                    decision = observeCadence(comparison);
                }
            }

            // A scene cut restarts the detector's windows but not the source
            // rate: keep the last published cadence so generation resumes on
            // the next distinct pair.
            if (const auto& estimate = cadence_.estimate())
            {
                frameSchedule_.setCadence(estimate->framesPerSecond);
            }

            generation = frameSchedule_.observe(decision);
        }

        bool generatedPresented = false;

        if (showsNewFrame && generation.generate && flow.submitted)
        {
            const auto presented =
                presentGenerated(sourceIndex, reflexFrameId, flow.output, true, generation.reset,
                                 generation.presentReal);

            if (!presented)
            {
                return std::unexpected(presented.error());
            }

            generatedPresented = *presented;
        }
        else if (generation.generate)
        {
            // Without this frame's flow there is no pair to interpolate; the
            // next distinct frame starts over.
            frameSchedule_.reset();
        }

        if (!generatedPresented || generation.presentReal)
        {
            if (auto presented = presentReal(reflexFrameId, generatedPresented); !presented)
            {
                return presented;
            }

            realPresented = true;
        }
    }
    else
    {
        if (marked)
        {
            reflex_.mark(reflexFrameId, LatencyMarker::PresentStart);
        }

        const HRESULT presentResult = swapChain_.present(requestedTiming_.load());
        presentGate_.observe(outcomeOf(presentResult));

        if (marked)
        {
            reflex_.mark(reflexFrameId, LatencyMarker::PresentEnd);
        }

        if (FAILED(presentResult))
        {
            return std::unexpected(describePresentFailure(presentResult));
        }

        realPresented = true;
    }

    gpu_.queue->Signal(fence_.Get(), ++lastSignaledFence_);
    allocatorFenceValues_.at(sourceIndex) = lastSignaledFence_;
    bufferShowedNewFrame_.at(sourceIndex) = showsNewFrame;
    framesInFlight_.at(sourceIndex) = shownFrame_;

    if (shownFrame_)
    {
        cadenceReferenceValid_ = true;

        if (plan.writeFlowPicture)
        {
            flowReferenceValid_ = true;
        }
    }

    const Clock::time_point now = Clock::now();
    const std::optional<Clock::duration> interval =
        showsNewFrame && lastNewFramePresentTime_
            ? std::optional(now - *lastNewFramePresentTime_)
            : std::nullopt;
    const std::optional<Clock::duration> readyToPresent =
        showsNewFrame ? std::optional(now - shownFrame_->timestamp) : std::nullopt;

    if (metrics_ != nullptr && realPresented)
    {
        metrics_->addPresented(now, interval, readyToPresent, showsNewFrame);
    }

    if (showsNewFrame && realPresented)
    {
        lastNewFramePresentTime_ = now;
        presented_.add(now - shownFrame_->timestamp);
    }

    reflexFrameId_.reset();
    reflex_.poll();
    return {};
}

std::expected<void, std::string> FrameRenderer::applyRequestedSize()
{
    const SurfaceSize requested = requestedSize_.load();

    if (requested == swapChain_.size())
    {
        return {};
    }

    waitForGpu();
    spdlog::debug("Resizing the swap chain to {}x{}", requested.width, requested.height);

    return swapChain_.resize(requested);
}

void FrameRenderer::applyRequestedReflex()
{
    const core::ReflexMode requested = requestedReflex_.load();

    if (requested == requestedReflexApplied_)
    {
        return;
    }

    requestedReflexApplied_ = requested;
    reflex_.configure(requested, gpu_.device.Get());
}

void FrameRenderer::applyRequestedUpscaling()
{
    const core::Upscaling requested = requestedUpscaling_.load();

    if (requested == requestedUpscalingApplied_)
    {
        return;
    }

    requestedUpscalingApplied_ = requested;
    videoSuperResolutionFailed_ = false;
    dlssFailed_ = false;
    dlssActive_ = false;
    dlssResetPending_ = true;
    dlssFallback_.clear();
    flowReferenceValid_ = false;
    flowSlotValid_.fill(false);
    // The real picture changes how it is composed; DLSS-G starts a new pair.
    frameSchedule_.reset();
    spdlog::info("Upscaling: {}", core::upscalingName(requested));
}

void FrameRenderer::applyRequestedHdr()
{
    const core::HdrMode requested = requestedHdr_.load();

    if (requested == requestedHdrApplied_)
    {
        return;
    }

    requestedHdrApplied_ = requested;
    rtxVideoHdrFailed_ = false;
    spdlog::info("HDR: {}", core::hdrModeName(requested));
}

void FrameRenderer::applyRequestedWindowLayout()
{
    const core::WindowLayout requested{
        .mode = requestedWindowMode_.load(),
        .size = requestedWindowSize_.load(),
    };

    if (requested == windowLayoutApplied_)
    {
        return;
    }

    windowLayoutApplied_ = requested;

    if (overlay_ != nullptr)
    {
        overlay_->setWindowLayout(requested.mode, requested.size);
    }
}

FrameRenderer::Presentation FrameRenderer::presentationFor(const VideoFrame* frame) const
{
    const TransferFunction transfer = frame != nullptr ? frame->transfer : TransferFunction::Sdr;
    const bool rtxAvailable = options_.rtxVideoHdrAvailable && !rtxVideoHdrFailed_;
    const HdrPresentation chosen =
        hdrPresentationFor(requestedHdr_.load(), display_, transfer, rtxAvailable);

    return {
        .targetFormat = chosen.hdrSwapChain ? SwapChain::kHdrFormat : SwapChain::kSdrFormat,
        .intermediateFormat = (chosen.hdrStream || chosen.rtxVideoHdr) ? SwapChain::kHdrFormat
                                                                        : kIntermediateFormat,
        .hdrSwapChain = chosen.hdrSwapChain,
        .toneMap = chosen.toneMap,
        .rtxVideoHdr = chosen.rtxVideoHdr,
        .hdrStream = chosen.hdrStream,
        .contentPeakNits = frame != nullptr ? contentPeakNitsOf(*frame) : 1000.0F,
        .sdrWhiteNits = static_cast<float>(display_.sdrWhiteNits),
    };
}

std::expected<void, std::string> FrameRenderer::applyPresentation(
    const Presentation& presentation)
{
    const bool targetChanged = presentation.targetFormat != presentation_.targetFormat;
    const bool intermediateChanged =
        presentation.intermediateFormat != presentation_.intermediateFormat;
    const bool pathChanged = presentation.hdrSwapChain != presentation_.hdrSwapChain ||
                             presentation.toneMap != presentation_.toneMap ||
                             presentation.rtxVideoHdr != presentation_.rtxVideoHdr;

    if (targetChanged || intermediateChanged)
    {
        waitForGpu();

        if (targetChanged)
        {
            if (auto resized = swapChain_.resize(swapChain_.size(), presentation.targetFormat);
                !resized)
            {
                return resized;
            }
        }

        auto created =
            FramePresenter::create(*gpu_.device.Get(), SwapChain::kBufferCount,
                                   presentation.intermediateFormat, presentation.targetFormat);

        if (!created)
        {
            return std::unexpected(created.error());
        }

        // The engine registered the old pictures: end its session first, then
        // release the pictures it read, each under a guard.
        opticalFlow_.reset();
        presenter_.releasePictures();
        presenter_ = std::move(*created);

        if (videoSuperResolution_)
        {
            for (UINT index = 0; index < SwapChain::kBufferCount; ++index)
            {
                presenter_.writeUpscaledView(index, *videoSuperResolution_->output(index));
            }
        }

        if (dlssSuperResolution_)
        {
            for (UINT index = 0; index < SwapChain::kBufferCount; ++index)
            {
                if (ID3D12Resource* const output = dlssSuperResolution_->output(index);
                    output != nullptr)
                {
                    presenter_.writeUpscaledView(index, *output);
                }
            }
        }

        flowReferenceValid_ = false;
        flowSlotValid_.fill(false);
        dlssResetPending_ = true;
        frameSchedule_.reset();
        frameSchedule_.setCadence(std::nullopt);

        if (targetChanged && overlay_ != nullptr)
        {
            overlay_->setOutputFormat(presentation.targetFormat, presentation.hdrSwapChain,
                                      presentation.sdrWhiteNits);
        }
    }

    if (pathChanged || !presentationLogged_)
    {
        logPresentation(presentation);
        presentationLogged_ = true;
    }

    presentation_ = presentation;
    return {};
}

void FrameRenderer::logPresentation(const Presentation& presentation) const
{
    if (presentation.hdrSwapChain)
    {
        spdlog::info("Presentation: HDR10 swap chain (R10G10B10A2_UNORM, "
                     "RGB_FULL_G2084_NONE_P2020), {}",
                     presentation.rtxVideoHdr ? "SDR stream converted by RTX Video HDR"
                                               : "PQ stream passed through");
        return;
    }

    if (presentation.toneMap)
    {
        spdlog::info("Presentation: SDR swap chain (R8G8B8A8_UNORM, "
                     "RGB_FULL_G22_NONE_P709), PQ stream tone-mapped to SDR (content peak {:.0f} "
                     "nits, SDR white {:.0f} nits)",
                     presentation.contentPeakNits, presentation.sdrWhiteNits);
        return;
    }

    spdlog::info("Presentation: SDR swap chain (R8G8B8A8_UNORM, RGB_FULL_G22_NONE_P709)");
}

bool FrameRenderer::prepareVideoSuperResolution(UINT bufferIndex)
{
    if (!shownFrame_)
    {
        return false;
    }

    const SurfaceSize input{.width = shownFrame_->width, .height = shownFrame_->height};
    const PixelRect area = fitPicture(swapChain_.size(), input);
    const SurfaceSize output{.width = area.width, .height = area.height};
    const bool upscaling = requestedUpscalingApplied_ != core::Upscaling::Shader;
    const bool superResolution =
        upscaling && !videoSuperResolutionFailed_ &&
        videoSuperResolutionEligible(true, gpu_.info.vendorId, input, output);
    const bool trueHdr = presentation_.rtxVideoHdr;

    if (!superResolution && !trueHdr)
    {
        return false;
    }

    if (gpu_.d3d11Device.Get() == nullptr || gpu_.d3d11Context.Get() == nullptr)
    {
        if (superResolution)
        {
            videoSuperResolutionFailed_ = true;
            spdlog::error("VSR is unavailable; the shader path scales the stream: the stream "
                          "adapter has no D3D11 device");
        }

        if (trueHdr)
        {
            rtxVideoHdrFailed_ = true;
            spdlog::error("RTX Video HDR is unavailable; the SDR shader path draws the stream: "
                          "the stream adapter has no D3D11 device");
        }

        return false;
    }

    const VsrPicture picture{
        .input = input,
        .output = output,
        .format = shownFrame_->format,
        .matrix = shownFrame_->matrix,
        .range = shownFrame_->range,
        .transfer = shownFrame_->transfer,
        .hdrOutput = presentation_.hdrStream || presentation_.rtxVideoHdr,
        .trueHdr = trueHdr,
        .superResolution = superResolution,
    };

    if (videoSuperResolution_ && videoSuperResolution_->configuredFor(picture))
    {
        preparedSuperResolution_ = superResolution;
        preparedTrueHdr_ = trueHdr;
        // The slot's view may have pointed at another upscaler's output.
        presenter_.writeUpscaledView(bufferIndex, *videoSuperResolution_->output(bufferIndex));
        return true;
    }

    waitForGpu();

    if (!videoSuperResolution_)
    {
        auto created =
            VideoSuperResolution::create(*gpu_.device.Get(), *gpu_.d3d11Device.Get(),
                                         *gpu_.d3d11Context.Get(), SwapChain::kBufferCount);

        if (!created)
        {
            if (superResolution)
            {
                videoSuperResolutionFailed_ = true;
            }

            if (trueHdr)
            {
                rtxVideoHdrFailed_ = true;
            }

            spdlog::error("The video processor is unavailable; the shader path draws the stream: "
                          "{}",
                          created.error());
            return false;
        }

        videoSuperResolution_.emplace(std::move(*created));
    }

    if (auto configured = videoSuperResolution_->configure(picture); !configured)
    {
        if (superResolution)
        {
            videoSuperResolutionFailed_ = true;
        }

        if (trueHdr)
        {
            rtxVideoHdrFailed_ = true;
            redrawRequested_.store(true);
            SetEvent(wakeEvent_.get());
        }

        spdlog::error("The video processor is unavailable; the shader path draws the stream: {}",
                      configured.error());
        return false;
    }

    for (UINT index = 0; index < SwapChain::kBufferCount; ++index)
    {
        presenter_.writeUpscaledView(index, *videoSuperResolution_->output(index));
    }

    preparedSuperResolution_ = superResolution;
    preparedTrueHdr_ = trueHdr;
    return true;
}

void FrameRenderer::applyRequestedOpticalFlow()
{
    const bool requested = requestedOpticalFlow_.load();

    if (requested == requestedOpticalFlowApplied_)
    {
        return;
    }

    requestedOpticalFlowApplied_ = requested;
    opticalFlowFailed_ = false;
    dlssFailed_ = false;
    flowReferenceValid_ = false;
    flowSlotValid_.fill(false);
    dlssResetPending_ = true;
    frameSchedule_.reset();
    spdlog::info("Optical flow: {}", requested ? "on" : "off");
}

void FrameRenderer::applyRequestedFrameGeneration()
{
    const core::FrameGenerationMode requested = requestedFrameGeneration_.load();

    if (requested == requestedFrameGenerationApplied_)
    {
        return;
    }

    requestedFrameGenerationApplied_ = requested;
    frameGenerationFailed_ = false;
    frameGenerationFailure_.clear();
    frameSchedule_.reset();

    if (requested == core::FrameGenerationMode::Off)
    {
        spdlog::info("Frame Generation: off");
        waitForGpu();
        releaseFrameGeneration();
        return;
    }

    spdlog::info("Frame Generation: on (2x), {}",
                 frameGenerationAvailability_.available
                     ? frameGenerationAvailability_.detail
                     : frameGenerationUnavailableReason(frameGenerationAvailability_.reason));

    if (!requestedOpticalFlowApplied_)
    {
        spdlog::info("Frame Generation: it needs the optical flow; it starts when the flow is "
                     "ready");
    }
}

void FrameRenderer::releaseOpticalFlow()
{
    if (!opticalFlow_)
    {
        return;
    }

    waitForGpu();

    if (flowTimer_)
    {
        flowTimer_->drain();
    }

    lastFlowMilliseconds_.reset();
    spdlog::info("Optical flow: the GPU is idle at fence {}; releasing the session",
                 lastSignaledFence_);
    opticalFlow_.reset();

    // The engine's registration goes with the session, but the driver can
    // fault on the last reference to the pictures it read; release them here,
    // one by one, so the fault is logged and the rest go.
    spdlog::info("Optical flow: releasing the pictures the engine read");
    presenter_.releasePictures();
}

void FrameRenderer::releaseFrameGeneration()
{
    frameGeneration_.reset();
    motionVectors_.reset();
    realPicture_.Reset();
    realPictureViews_.Reset();
    realPictureView_ = {};
    realPictureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    generationAllocatorFenceValues_.fill(0);
    presentAllocatorFenceValues_.fill(0);
}

std::expected<NgxHost*, std::string> FrameRenderer::ensureNgxHost()
{
    if (!ngxHost_)
    {
        auto host = NgxHost::create(*gpu_.device.Get());

        if (!host)
        {
            return std::unexpected(host.error());
        }

        ngxHost_ = std::move(*host);
    }

    return &*ngxHost_;
}

bool FrameRenderer::prepareDlssSuperResolution(UINT bufferIndex, bool flowThisFrame)
{
    if (requestedUpscalingApplied_ != core::Upscaling::Dlss)
    {
        return false;
    }

    if (!shownFrame_)
    {
        return false;
    }

    if (presentation_.hdrSwapChain)
    {
        noteDlssFallback("the HDR display needs VSR's 10-bit output");
        return false;
    }

    const SurfaceSize input{.width = shownFrame_->width, .height = shownFrame_->height};
    const PixelRect area = fitPicture(swapChain_.size(), input);

    if (!dlssSuperResolutionEligible(input, {.width = area.width, .height = area.height}))
    {
        noteDlssFallback(
            std::format("{}x{} in a {}x{} window is outside DLSS's upscale range", input.width,
                        input.height, swapChain_.size().width, swapChain_.size().height));
        return false;
    }

    if (!requestedOpticalFlowApplied_ || opticalFlowFailed_)
    {
        noteDlssFallback("the optical flow is off");
        return false;
    }

    if (!flowReferenceValid_ || (!flowThisFrame && !flowSlotValid_.at(bufferIndex)))
    {
        noteDlssFallback("waiting for the first optical flow vectors");
        return false;
    }

    if (dlssFailed_)
    {
        return false;
    }

    const auto host = ensureNgxHost();

    if (!host)
    {
        dlssFailed_ = true;
        noteDlssFallback(host.error());
        return false;
    }

    if (!dlssSuperResolution_)
    {
        auto created = DlssSuperResolution::create(**host, *gpu_.queue.Get(),
                                                   SwapChain::kBufferCount);

        if (!created)
        {
            dlssFailed_ = true;
            noteDlssFallback(created.error());
            return false;
        }

        dlssSuperResolution_ = std::move(*created);
    }

    const DlssPicture picture{
        .input = input,
        .output = {.width = area.width, .height = area.height},
        .toneMappedInput = presentation_.intermediateFormat != kIntermediateFormat,
    };

    if (!dlssSuperResolution_->configuredFor(picture))
    {
        waitForGpu();

        if (auto configured = dlssSuperResolution_->configure(picture); !configured)
        {
            dlssFailed_ = true;
            noteDlssFallback(configured.error());
            return false;
        }

        for (UINT index = 0; index < SwapChain::kBufferCount; ++index)
        {
            if (ID3D12Resource* const output = dlssSuperResolution_->output(index);
                output != nullptr)
            {
                presenter_.writeUpscaledView(index, *output);
            }
        }

        dlssResetPending_ = true;
    }

    if (!dlssFallback_.empty())
    {
        dlssFallback_.clear();
        spdlog::info("DLSS SR: on ({}x{} -> {}x{})", picture.input.width, picture.input.height,
                     picture.output.width, picture.output.height);
    }

    dlssActive_ = true;
    return true;
}

void FrameRenderer::noteDlssFallback(std::string reason)
{
    if (reason == dlssFallback_)
    {
        return;
    }

    dlssFallback_ = std::move(reason);
    spdlog::info("DLSS SR: {}; VSR draws the stream", dlssFallback_);
}

bool FrameRenderer::prepareFrameGeneration(UINT bufferIndex)
{
    if (requestedFrameGenerationApplied_ != core::FrameGenerationMode::X2 ||
        frameGenerationFailed_ || !shownFrame_)
    {
        return false;
    }

    if (!frameGenerationAvailability_.available)
    {
        frameGenerationFailed_ = true;
        frameGenerationFailure_ = frameGenerationAvailability_.detail;
        spdlog::info("Frame Generation: the RTX screen's availability check refused it ({})",
                     frameGenerationFailure_);
        return false;
    }

    if (cadenceMetric_ == nullptr)
    {
        frameGenerationFailed_ = true;
        frameGenerationFailure_ = "the cadence metric is unavailable";
        spdlog::info("Frame Generation: it needs the cadence detection, which is unavailable");
        return false;
    }

    const SurfaceSize picture{.width = shownFrame_->width, .height = shownFrame_->height};

    if (!requestedOpticalFlowApplied_)
    {
        return false;
    }

    if (opticalFlowFailed_)
    {
        frameGenerationFailed_ = true;
        frameGenerationFailure_ = "the optical flow is unavailable";
        spdlog::info("Frame Generation: it needs the optical flow, which failed");
        return false;
    }

    if (!opticalFlow_ || !opticalFlow_->configuredFor(picture))
    {
        return false;
    }

    // Frame Generation needs Reflex's low-latency mode; enforce it here too,
    // since the settings file may predate the pairing.
    if (requestedReflex_.load() == core::ReflexMode::Off)
    {
        requestedReflex_.store(core::ReflexMode::On);
        applyRequestedReflex();
        spdlog::info("Frame Generation: Reflex turned on (required by the feature)");
    }

    if (!reflex_.available() || reflex_.failed())
    {
        frameGenerationFailed_ = true;
        frameGenerationFailure_ = "Reflex is unavailable";
        spdlog::info("Frame Generation: it needs the NVIDIA Reflex, which is unavailable here");
        return false;
    }

    if (reflex_.mode() == core::ReflexMode::Off)
    {
        reflex_.configure(core::ReflexMode::On, gpu_.device.Get());

        if (reflex_.mode() == core::ReflexMode::Off)
        {
            frameGenerationFailed_ = true;
            frameGenerationFailure_ = "Reflex refused the low-latency mode";
            return false;
        }
    }

    const SurfaceSize extent = swapChain_.size();
    const DXGI_FORMAT format = presentation_.targetFormat;
    std::array<ID3D12Resource*, SwapChain::kBufferCount> flows{};

    for (UINT index = 0; index < SwapChain::kBufferCount; ++index)
    {
        flows.at(index) = opticalFlow_->flow(index);

        if (flows.at(index) == nullptr)
        {
            return false;
        }
    }

    const bool ready =
        frameGeneration_ && frameGeneration_->configuredFor(extent, format) && motionVectors_ &&
        motionVectors_->configuredFor(picture, extent, flows) && realPicture_;

    if (ready)
    {
        return true;
    }

    waitForGpu();
    releaseFrameGeneration();
    // A new feature has no temporal history.
    frameSchedule_.reset();

    const auto host = ensureNgxHost();

    if (!host)
    {
        frameGenerationFailed_ = true;
        frameGenerationFailure_ = host.error();
        spdlog::info("Frame Generation: NGX is unavailable: {}", frameGenerationFailure_);
        return false;
    }

    if (!reflex_.outOfBandEnabled() && !reflex_.enableOutOfBand(*gpu_.queue.Get()))
    {
        frameGenerationFailed_ = true;
        frameGenerationFailure_ = "the driver refused the out-of-band queue notification";
        spdlog::info("Frame Generation: {}", frameGenerationFailure_);
        return false;
    }

    const D3D12_HEAP_PROPERTIES defaultHeap{.Type = D3D12_HEAP_TYPE_DEFAULT};
    const D3D12_RESOURCE_DESC description{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = extent.width,
        .Height = extent.height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = format,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
    };

    if (const HRESULT result = gpu_.device->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &description,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&realPicture_));
        FAILED(result))
    {
        frameGenerationFailed_ = true;
        frameGenerationFailure_ = describeFailure("CreateCommittedResource (real picture)", result);
        spdlog::info("Frame Generation: {}", frameGenerationFailure_);
        return false;
    }

    const D3D12_DESCRIPTOR_HEAP_DESC viewHeapDescription{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = 1,
    };

    if (const HRESULT result = gpu_.device->CreateDescriptorHeap(
            &viewHeapDescription, IID_PPV_ARGS(&realPictureViews_));
        FAILED(result))
    {
        frameGenerationFailed_ = true;
        frameGenerationFailure_ = describeFailure("CreateDescriptorHeap (real picture)", result);
        spdlog::info("Frame Generation: {}", frameGenerationFailure_);
        return false;
    }

    realPictureView_ = realPictureViews_->GetCPUDescriptorHandleForHeapStart();
    gpu_.device->CreateRenderTargetView(realPicture_.Get(), nullptr, realPictureView_);
    realPictureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    auto vectors = MotionVectors::create(*gpu_.device.Get(), SwapChain::kBufferCount);

    if (!vectors)
    {
        frameGenerationFailed_ = true;
        frameGenerationFailure_ = vectors.error();
        spdlog::info("Frame Generation: {}", frameGenerationFailure_);
        return false;
    }

    if (auto configured = vectors->configure(picture, extent, flows); !configured)
    {
        frameGenerationFailed_ = true;
        frameGenerationFailure_ = configured.error();
        spdlog::info("Frame Generation: {}", frameGenerationFailure_);
        return false;
    }

    motionVectors_.emplace(std::move(*vectors));

    waitForFence(generationAllocatorFenceValues_.at(bufferIndex));
    generationAllocators_.at(bufferIndex)->Reset();
    generationCommandList_->Reset(generationAllocators_.at(bufferIndex).Get(), nullptr);

    auto feature = FrameGeneration::create(**host, *gpu_.device.Get(),
                                           *generationCommandList_.Get(), extent, format);

    if (const HRESULT result = generationCommandList_->Close(); FAILED(result))
    {
        frameGenerationFailed_ = true;
        frameGenerationFailure_ =
            describeFailure("ID3D12GraphicsCommandList::Close (frame generation)", result);
        spdlog::info("Frame Generation: {}", frameGenerationFailure_);
        return false;
    }

    if (!feature)
    {
        // The failed feature released its resources, so the recorded list is
        // discarded instead of executed.
        generationAllocators_.at(bufferIndex)->Reset();
        generationCommandList_->Reset(generationAllocators_.at(bufferIndex).Get(), nullptr);
        generationCommandList_->Close();

        frameGenerationFailed_ = true;
        frameGenerationFailure_ = feature.error();
        spdlog::info("Frame Generation: {}", frameGenerationFailure_);
        return false;
    }

    const std::array<ID3D12CommandList*, 1> lists{generationCommandList_.Get()};
    gpu_.queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());
    gpu_.queue->Signal(fence_.Get(), ++lastSignaledFence_);
    generationAllocatorFenceValues_.at(bufferIndex) = lastSignaledFence_;
    waitForFence(lastSignaledFence_);
    frameGeneration_.emplace(std::move(*feature));

    // The runtime allocates its internals at the first evaluate and can
    // refuse it once the command list has other views bound; warm the
    // feature up once, with a reset, before the first real pair.
    generationAllocators_.at(bufferIndex)->Reset();
    generationCommandList_->Reset(generationAllocators_.at(bufferIndex).Get(), nullptr);

    const auto realToShader =
        transition(realPicture_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    generationCommandList_->ResourceBarrier(1, &realToShader);

    const auto warmed =
        frameGeneration_->evaluate(*generationCommandList_.Get(), *realPicture_.Get(),
                                   *motionVectors_->texture(), true, 0);

    if (!warmed)
    {
        // The failed evaluate may have left the generated texture in UAV, so
        // the recorded list is discarded instead of executed.
        generationCommandList_->Close();
        generationAllocators_.at(bufferIndex)->Reset();
        generationCommandList_->Reset(generationAllocators_.at(bufferIndex).Get(), nullptr);
        generationCommandList_->Close();

        frameGenerationFailed_ = true;
        frameGenerationFailure_ = warmed.error();
        spdlog::info("Frame Generation: the warm-up evaluate failed: {}",
                     frameGenerationFailure_);
        frameGeneration_.reset();
        return false;
    }

    const auto generatedToCommon = transition(
        frameGeneration_->generated(), D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_COMMON);
    generationCommandList_->ResourceBarrier(1, &generatedToCommon);

    const auto realBack =
        transition(realPicture_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    generationCommandList_->ResourceBarrier(1, &realBack);

    if (const HRESULT result = generationCommandList_->Close(); FAILED(result))
    {
        frameGenerationFailed_ = true;
        frameGenerationFailure_ =
            describeFailure("ID3D12GraphicsCommandList::Close (frame generation warm-up)",
                            result);
        spdlog::info("Frame Generation: {}", frameGenerationFailure_);
        return false;
    }

    const std::array<ID3D12CommandList*, 1> warmLists{generationCommandList_.Get()};
    gpu_.queue->ExecuteCommandLists(static_cast<UINT>(warmLists.size()), warmLists.data());
    gpu_.queue->Signal(fence_.Get(), ++lastSignaledFence_);
    generationAllocatorFenceValues_.at(bufferIndex) = lastSignaledFence_;
    waitForFence(lastSignaledFence_);

    frameGeneration_->generatedCopied();
    spdlog::info("Frame Generation: {} created for {}x{} {} ({} warm-up evaluations)",
                 frameGeneration_->description(), extent.width, extent.height,
                 static_cast<int>(format), frameGeneration_->evaluations());
    return true;
}

FrameRenderer::FrameTarget FrameRenderer::frameTarget(UINT bufferIndex, bool offscreen)
{
    if (offscreen)
    {
        return {
            .resource = realPicture_.Get(),
            .view = realPictureView_,
            .before = realPictureState_,
            .after = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        };
    }

    return {
        .resource = swapChain_.backBuffer(bufferIndex),
        .view = swapChain_.renderTargetView(bufferIndex),
        .before = D3D12_RESOURCE_STATE_PRESENT,
        .after = D3D12_RESOURCE_STATE_PRESENT,
    };
}

std::expected<bool, std::string> FrameRenderer::presentGenerated(
    UINT motionVectorsSlot, std::uint64_t frameId, std::uint64_t flowOutput, bool waitForFlow,
    bool reset, bool keepReal)
{
    if (!frameGeneration_ || !motionVectors_ || !realPicture_)
    {
        return std::unexpected("Frame Generation isn't prepared");
    }

    // The render loop's wait already granted this frame's first present.
    const UINT index = swapChain_.currentIndex();
    waitForFence(generationAllocatorFenceValues_.at(index));
    generationAllocators_.at(index)->Reset();
    generationCommandList_->Reset(generationAllocators_.at(index).Get(), nullptr);

    ID3D12GraphicsCommandList& list = *generationCommandList_.Get();

    if (waitForFlow && opticalFlow_)
    {
        gpu_.queue->Wait(opticalFlow_->fence(), flowOutput);
    }

    motionVectors_->record(list, motionVectorsSlot);

    const auto evaluated = frameGeneration_->evaluate(list, *realPicture_.Get(),
                                                      *motionVectors_->texture(), reset, frameId);

    if (!evaluated)
    {
        generationCommandList_->Close();
        generationAllocators_.at(index)->Reset();
        generationCommandList_->Reset(generationAllocators_.at(index).Get(), nullptr);
        generationCommandList_->Close();

        frameGenerationFailed_ = true;
        frameGenerationFailure_ = evaluated.error();
        redrawRequested_.store(true);
        SetEvent(wakeEvent_.get());
        spdlog::error("Frame Generation stopped; the stream continues without generated frames: "
                      "{}",
                      frameGenerationFailure_);
        return false;
    }

    ID3D12Resource* const backBuffer = swapChain_.backBuffer(index);
    ID3D12Resource* const generated = frameGeneration_->generated();

    const auto toCopy = transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT,
                                   D3D12_RESOURCE_STATE_COPY_DEST);
    list.ResourceBarrier(1, &toCopy);
    list.CopyResource(backBuffer, generated);

    const auto toTarget = transition(backBuffer, D3D12_RESOURCE_STATE_COPY_DEST,
                                     D3D12_RESOURCE_STATE_RENDER_TARGET);
    list.ResourceBarrier(1, &toTarget);

    if (overlay_ != nullptr && overlay_->visible())
    {
        overlay_->render(list, swapChain_.renderTargetView(index), overlayStats(), gamepadState());
    }

    const auto toPresent = transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                      D3D12_RESOURCE_STATE_PRESENT);
    list.ResourceBarrier(1, &toPresent);

    const auto generatedBackToCommon = transition(
        generated, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
    list.ResourceBarrier(1, &generatedBackToCommon);
    frameGeneration_->generatedCopied();

    const D3D12_RESOURCE_STATES realNext =
        keepReal ? D3D12_RESOURCE_STATE_COPY_SOURCE : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    const auto realToNext =
        transition(realPicture_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, realNext);
    list.ResourceBarrier(1, &realToNext);
    realPictureState_ = realNext;

    if (const HRESULT result = generationCommandList_->Close(); FAILED(result))
    {
        return std::unexpected(
            describeFailure("ID3D12GraphicsCommandList::Close (generated present)", result));
    }

    reflex_.markOutOfBand(frameId, LatencyMarker::OutOfBandRenderSubmitStart);
    const std::array<ID3D12CommandList*, 1> lists{generationCommandList_.Get()};
    gpu_.queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());
    reflex_.markOutOfBand(frameId, LatencyMarker::OutOfBandRenderSubmitEnd);

    gpu_.queue->Signal(fence_.Get(), ++lastSignaledFence_);
    generationAllocatorFenceValues_.at(index) = lastSignaledFence_;

    reflex_.markOutOfBand(frameId, LatencyMarker::OutOfBandPresentStart);
    const HRESULT presentResult = swapChain_.present(requestedTiming_.load());
    reflex_.markOutOfBand(frameId, LatencyMarker::OutOfBandPresentEnd);
    presentGate_.observe(outcomeOf(presentResult));

    if (FAILED(presentResult))
    {
        return std::unexpected(describePresentFailure(presentResult));
    }

    ++generatedFrames_;

    if (metrics_ != nullptr)
    {
        metrics_->addGeneratedPresent(Clock::now());
    }

    spdlog::debug("frame generation: evaluated source frame {} into generated picture {}",
                  frameId, generatedFrames_);
    return true;
}

std::expected<void, std::string> FrameRenderer::presentReal(std::uint64_t frameId,
                                                            bool secondPresent)
{
    if (!realPicture_)
    {
        return std::unexpected("Frame Generation's real picture isn't prepared");
    }

    if (secondPresent)
    {
        (void)swapChain_.waitForFrameSlot(kFrameSlotTimeout);
    }

    const UINT index = swapChain_.currentIndex();
    waitForFence(presentAllocatorFenceValues_.at(index));
    presentAllocators_.at(index)->Reset();
    generationCommandList_->Reset(presentAllocators_.at(index).Get(), nullptr);

    ID3D12GraphicsCommandList& list = *generationCommandList_.Get();
    ID3D12Resource* const backBuffer = swapChain_.backBuffer(index);

    if (realPictureState_ != D3D12_RESOURCE_STATE_COPY_SOURCE)
    {
        const auto toSource =
            transition(realPicture_.Get(), realPictureState_, D3D12_RESOURCE_STATE_COPY_SOURCE);
        list.ResourceBarrier(1, &toSource);
    }

    const auto toCopy = transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT,
                                   D3D12_RESOURCE_STATE_COPY_DEST);
    list.ResourceBarrier(1, &toCopy);
    list.CopyResource(backBuffer, realPicture_.Get());

    const auto toPresent = transition(backBuffer, D3D12_RESOURCE_STATE_COPY_DEST,
                                      D3D12_RESOURCE_STATE_PRESENT);
    list.ResourceBarrier(1, &toPresent);

    const auto toShader = transition(realPicture_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                                     D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    list.ResourceBarrier(1, &toShader);
    realPictureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    if (const HRESULT result = generationCommandList_->Close(); FAILED(result))
    {
        return std::unexpected(
            describeFailure("ID3D12GraphicsCommandList::Close (real present)", result));
    }

    const std::array<ID3D12CommandList*, 1> lists{generationCommandList_.Get()};
    gpu_.queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());
    gpu_.queue->Signal(fence_.Get(), ++lastSignaledFence_);
    presentAllocatorFenceValues_.at(index) = lastSignaledFence_;

    if (frameId != 0)
    {
        reflex_.mark(frameId, LatencyMarker::PresentStart);
    }

    const HRESULT presentResult = swapChain_.present(requestedTiming_.load());
    presentGate_.observe(outcomeOf(presentResult));

    if (frameId != 0)
    {
        reflex_.mark(frameId, LatencyMarker::PresentEnd);
    }

    if (FAILED(presentResult))
    {
        return std::unexpected(describePresentFailure(presentResult));
    }

    return {};
}

bool FrameRenderer::prepareOpticalFlow()
{
    if (!shownFrame_ || !requestedOpticalFlowApplied_ || opticalFlowFailed_)
    {
        return false;
    }

    const SurfaceSize picture{.width = shownFrame_->width, .height = shownFrame_->height};

    if (opticalFlow_ && opticalFlow_->configuredFor(picture))
    {
        return true;
    }

    waitForGpu();

    if (!opticalFlow_)
    {
        auto created = OpticalFlow::create(*gpu_.device.Get(), SwapChain::kBufferCount);

        if (!created)
        {
            opticalFlowFailed_ = true;
            spdlog::error("Optical flow is unavailable; cadence runs on the block metric alone: {}",
                          created.error());
            return false;
        }

        opticalFlow_.emplace(std::move(*created));
        spdlog::info("Optical flow: {}", opticalFlow_->description());
    }

    std::array<ID3D12Resource*, SwapChain::kBufferCount> inputs{};

    for (UINT index = 0; index < SwapChain::kBufferCount; ++index)
    {
        inputs.at(index) = presenter_.flowIntermediate(index);

        if (inputs.at(index) == nullptr)
        {
            opticalFlowFailed_ = true;
            spdlog::error("Optical flow is unavailable; the BGRA8 pictures are missing");
            return false;
        }
    }

    if (auto configured = opticalFlow_->configure(picture, inputs); !configured)
    {
        opticalFlowFailed_ = true;
        spdlog::error("Optical flow is unavailable; cadence runs on the block metric alone: {}",
                      configured.error());
        return false;
    }

    flowReferenceValid_ = false;
    flowSlotValid_.fill(false);
    spdlog::info("Optical flow: {}", opticalFlow_->description());
    return true;
}

void FrameRenderer::signalCadence()
{
    cadenceFrameValue_ = std::exchange(cadenceSignal_, 0);

    if (cadenceFrameValue_ == 0 || cadenceMetric_ == nullptr)
    {
        return;
    }

    gpu_.queue->Signal(cadenceMetric_->fence(), cadenceFrameValue_);
}

void FrameRenderer::drainCadence()
{
    if (cadenceMetric_ == nullptr)
    {
        return;
    }

    while (const auto sample = cadenceMetric_->take())
    {
        const double blockSamples =
            static_cast<double>(sample->totalBlocks) * CadenceMetric::kSamplesPerBlock;

        const CadenceSample comparison{
            .changedBlocks = sample->changedBlocks,
            .totalBlocks = sample->totalBlocks,
            .meanDifference =
                blockSamples > 0.0 ? static_cast<double>(sample->sad) / blockSamples : 0.0,
        };

        if (observeCadence(comparison) == CadenceDetector::Decision::SceneCut)
        {
            dlssResetPending_ = true;
            frameSchedule_.reset();
            spdlog::debug("Cadence: scene cut, window restarted");
        }
    }
}

CadenceDetector::Decision FrameRenderer::observeCadence(const CadenceSample& sample)
{
    const CadenceDetector::Decision decision = cadence_.observe(sample);

    if (metrics_ != nullptr)
    {
        // A scene cut is still a new picture, though the detector restarts
        // its windows around it.
        const bool distinct = decision == CadenceDetector::Decision::Distinct ||
                              decision == CadenceDetector::Decision::SceneCut;
        metrics_->addCadenceFrame(Clock::now(), distinct);
    }

    return decision;
}

void FrameRenderer::timeOpticalFlow(UINT bufferIndex, std::uint64_t inputValue,
                                    std::uint64_t outputValue)
{
    if (!flowTimer_)
    {
        return;
    }

    if (const auto queued =
            flowTimer_->submit(bufferIndex, *opticalFlow_->fence(), inputValue, outputValue);
        !queued)
    {
        spdlog::warn("The optical flow's own time stopped being measured: {}", queued.error());
        flowTimer_.reset();
    }
}

StageOverlayState FrameRenderer::opticalFlowState() const
{
    if (!requestedOpticalFlowApplied_)
    {
        return StageOverlayState::Off;
    }

    if (opticalFlowFailed_)
    {
        return StageOverlayState::Unavailable;
    }

    if (!opticalFlow_ || !shownFrame_)
    {
        return StageOverlayState::Off;
    }

    const SurfaceSize picture{.width = shownFrame_->width, .height = shownFrame_->height};
    return opticalFlow_->configuredFor(picture) ? StageOverlayState::Running
                                                 : StageOverlayState::Off;
}

StageOverlayState FrameRenderer::dlssState() const
{
    if (requestedUpscalingApplied_ != core::Upscaling::Dlss)
    {
        return StageOverlayState::Off;
    }

    if (dlssFailed_ || !dlssActive_)
    {
        return StageOverlayState::Unavailable;
    }

    return StageOverlayState::Running;
}

ToneMapParameters FrameRenderer::flowToneMap() const
{
    return {
        .enabled = presentation_.hdrStream,
        .contentPeakNits = presentation_.contentPeakNits,
        .sdrWhiteNits = presentation_.sdrWhiteNits,
    };
}

void FrameRenderer::recordShaderFrame(UINT bufferIndex, const FramePlan& plan,
                                      const FrameTarget& target)
{
    const D3D12_CPU_DESCRIPTOR_HANDLE targetView = target.view;

    timer_->beginFrame(*commandList_.Get(), bufferIndex);

    // The shader path doesn't run the video processor; those stages still
    // need valid stamps for the frame's times to be read back.
    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::RtxVideoHdr);
    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::RtxVideoHdr);
    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::VideoSuperResolution);
    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::VideoSuperResolution);
    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::DlssSuperResolution);
    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::DlssSuperResolution);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::ColorConversion);

    if (shownFrame_)
    {
        presenter_.recordConversion(*commandList_.Get(), bufferIndex, *shownFrame_);

        if (plan.writeFlowPicture)
        {
            presenter_.recordFlowPicture(*commandList_.Get(), bufferIndex, flowToneMap());
        }
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::ColorConversion);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Cadence);

    if (plan.cadence && cadenceMetric_)
    {
        cadenceSignal_ =
            cadenceMetric_->record(*commandList_.Get(), *shownFrame_, *plan.previous);
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Cadence);

    const auto toRenderTarget =
        transition(target.resource, target.before, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &toRenderTarget);
    commandList_->ClearRenderTargetView(targetView, kBlack.data(), 0, nullptr);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Scaling);

    if (shownFrame_)
    {
        const SurfaceSize picture{.width = shownFrame_->width, .height = shownFrame_->height};
        const ToneMapParameters toneMap{
            .enabled = presentation_.toneMap,
            .contentPeakNits = presentation_.contentPeakNits,
            .sdrWhiteNits = presentation_.sdrWhiteNits,
        };
        presenter_.recordScaling(*commandList_.Get(), bufferIndex, targetView, swapChain_.size(),
                                  picture, ScaledPicture::Converted, toneMap);
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Scaling);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    if (overlay_ != nullptr)
    {
        overlay_->render(*commandList_.Get(), targetView, overlayStats(), gamepadState());
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    const auto toNext =
        transition(target.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, target.after);
    commandList_->ResourceBarrier(1, &toNext);

    if (target.resource == realPicture_.Get())
    {
        realPictureState_ = target.after;
    }

    timer_->endFrame(*commandList_.Get(), bufferIndex);
}

std::expected<void, std::string> FrameRenderer::recordDlssSuperResolutionFrame(
    UINT bufferIndex, const FramePlan& plan, const FrameTarget& target, FrameFlow& flow)
{
    const SurfaceSize picture{.width = shownFrame_->width, .height = shownFrame_->height};
    const PixelRect area = fitPicture(swapChain_.size(), picture);
    const SurfaceSize output{.width = area.width, .height = area.height};
    const bool toneMappedInput = presentation_.intermediateFormat != kIntermediateFormat;

    // First submission: the color conversion (and the SDR copy a 10-bit
    // intermediate needs), the cadence sample and, for a new frame, the flow
    // engine. DLSS waits for that engine before it runs.
    copyAllocators_.at(bufferIndex)->Reset();
    copyCommandList_->Reset(copyAllocators_.at(bufferIndex).Get(), nullptr);
    timer_->beginFrame(*copyCommandList_.Get(), bufferIndex);

    timer_->beginStage(*copyCommandList_.Get(), bufferIndex, EffectStage::RtxVideoHdr);
    timer_->endStage(*copyCommandList_.Get(), bufferIndex, EffectStage::RtxVideoHdr);
    timer_->beginStage(*copyCommandList_.Get(), bufferIndex, EffectStage::VideoSuperResolution);
    timer_->endStage(*copyCommandList_.Get(), bufferIndex, EffectStage::VideoSuperResolution);

    timer_->beginStage(*copyCommandList_.Get(), bufferIndex, EffectStage::ColorConversion);
    presenter_.recordConversion(*copyCommandList_.Get(), bufferIndex, *shownFrame_);

    if (plan.writeFlowPicture && toneMappedInput)
    {
        presenter_.recordFlowPicture(*copyCommandList_.Get(), bufferIndex, flowToneMap());
    }

    timer_->endStage(*copyCommandList_.Get(), bufferIndex, EffectStage::ColorConversion);

    timer_->beginStage(*copyCommandList_.Get(), bufferIndex, EffectStage::Cadence);

    if (plan.cadence && cadenceMetric_)
    {
        cadenceSignal_ = cadenceMetric_->record(*copyCommandList_.Get(), *shownFrame_,
                                                 *plan.previous);
    }

    timer_->endStage(*copyCommandList_.Get(), bufferIndex, EffectStage::Cadence);

    if (const HRESULT result = copyCommandList_->Close(); FAILED(result))
    {
        return std::unexpected(describeFailure("ID3D12GraphicsCommandList::Close (DLSS)", result));
    }

    if (shownFrame_->fence)
    {
        gpu_.queue->Wait(shownFrame_->fence.Get(), shownFrame_->fenceValue);
    }

    const std::array<ID3D12CommandList*, 1> copyLists{copyCommandList_.Get()};
    gpu_.queue->ExecuteCommandLists(static_cast<UINT>(copyLists.size()), copyLists.data());
    signalCadence();

    copyAllocatorFenceValues_.at(bufferIndex) = ++lastSignaledFence_;
    gpu_.queue->Signal(fence_.Get(), lastSignaledFence_);

    bool waitForFlow = false;
    bool flowFailed = false;

    if (plan.opticalFlow)
    {
        const std::uint64_t inputValue = opticalFlow_->nextInputValue();
        gpu_.queue->Signal(opticalFlow_->fence(), inputValue);

        const auto flowValue = opticalFlow_->execute(bufferIndex, inputValue);
        flow.consumed = true;

        if (flowValue)
        {
            flow.submitted = true;
            flow.output = *flowValue;
            waitForFlow = true;
            flowSlotValid_.at(bufferIndex) = true;
            timeOpticalFlow(bufferIndex, inputValue, *flowValue);
        }
        else
        {
            flowFailed = true;
            opticalFlowFailed_ = true;
            dlssResetPending_ = true;
            noteDlssFallback(flowValue.error());
        }
    }

    // Second submission: DLSS with the vectors in, then the draw. When the
    // feature fails, the converted picture is drawn instead, so the frame
    // still reaches the screen.
    allocators_.at(bufferIndex)->Reset();
    commandList_->Reset(allocators_.at(bufferIndex).Get(), nullptr);

    if (waitForFlow)
    {
        gpu_.queue->Wait(opticalFlow_->fence(), flow.output);
    }
    else if (const std::uint64_t lastFlow = opticalFlow_->lastOutputValue(); lastFlow != 0)
    {
        // A redraw reads this slot's last vectors; waiting on the engine's
        // newest value covers every flow it has written.
        gpu_.queue->Wait(opticalFlow_->fence(), lastFlow);
    }

    bool drewDlss = false;

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::DlssSuperResolution);

    if (!flowFailed)
    {
        ID3D12Resource* const color = presenter_.flowIntermediate(bufferIndex);
        ID3D12Resource* const flowGrid = opticalFlow_->flow(bufferIndex);

        if (color != nullptr && flowGrid != nullptr)
        {
            const bool reset = std::exchange(dlssResetPending_, false);

            if (auto recorded = dlssSuperResolution_->record(*commandList_.Get(), bufferIndex,
                                                             *color, *flowGrid, reset);
                recorded)
            {
                drewDlss = true;
            }
            else
            {
                dlssFailed_ = true;
                dlssResetPending_ = true;
                noteDlssFallback(recorded.error());
            }
        }
        else
        {
            dlssFailed_ = true;
            noteDlssFallback("the picture or the flow textures are missing");
        }
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::DlssSuperResolution);
    dlssActive_ = drewDlss;

    if (drewDlss)
    {
        if (ID3D12Resource* const upscaled = dlssSuperResolution_->output(bufferIndex);
            upscaled != nullptr)
        {
            presenter_.writeUpscaledView(bufferIndex, *upscaled);
        }
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE targetView = target.view;

    const auto toRenderTarget =
        transition(target.resource, target.before, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &toRenderTarget);
    commandList_->ClearRenderTargetView(targetView, kBlack.data(), 0, nullptr);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Scaling);

    // DLSS writes SDR; the fallback draws the (possibly PQ) intermediate,
    // which the scaling pass tone-maps for an SDR target.
    const ToneMapParameters dlssToneMap{
        .enabled = false,
        .contentPeakNits = presentation_.contentPeakNits,
        .sdrWhiteNits = presentation_.sdrWhiteNits,
    };
    const ToneMapParameters convertedToneMap{
        .enabled = presentation_.toneMap,
        .contentPeakNits = presentation_.contentPeakNits,
        .sdrWhiteNits = presentation_.sdrWhiteNits,
    };

    presenter_.recordScaling(*commandList_.Get(), bufferIndex, targetView, swapChain_.size(),
                              drewDlss ? output : picture,
                              drewDlss ? ScaledPicture::Upscaled : ScaledPicture::Converted,
                              drewDlss ? dlssToneMap : convertedToneMap);
    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Scaling);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    if (overlay_ != nullptr)
    {
        overlay_->render(*commandList_.Get(), targetView, overlayStats(), gamepadState());
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    const auto toNext =
        transition(target.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, target.after);
    commandList_->ResourceBarrier(1, &toNext);

    if (target.resource == realPicture_.Get())
    {
        realPictureState_ = target.after;
    }

    timer_->endFrame(*commandList_.Get(), bufferIndex);

    if (const HRESULT result = commandList_->Close(); FAILED(result))
    {
        return std::unexpected(describeFailure("ID3D12GraphicsCommandList::Close (DLSS)", result));
    }

    const std::array<ID3D12CommandList*, 1> lists{commandList_.Get()};
    gpu_.queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());
    return {};
}

std::expected<bool, std::string> FrameRenderer::recordVideoSuperResolutionFrame(
    UINT bufferIndex, const FramePlan& plan, const FrameTarget& target)
{
    const SurfaceSize picture{.width = shownFrame_->width, .height = shownFrame_->height};

    copyAllocators_.at(bufferIndex)->Reset();
    copyCommandList_->Reset(copyAllocators_.at(bufferIndex).Get(), nullptr);
    timer_->beginFrame(*copyCommandList_.Get(), bufferIndex);

    // Each model's stage runs from the copy that feeds it to the blit that
    // applies it; both models share one blit when both are on.
    if (preparedTrueHdr_)
    {
        timer_->beginStage(*copyCommandList_.Get(), bufferIndex, EffectStage::RtxVideoHdr);
    }

    if (preparedSuperResolution_)
    {
        timer_->beginStage(*copyCommandList_.Get(), bufferIndex,
                           EffectStage::VideoSuperResolution);
    }

    recordInputCopy(*copyCommandList_.Get(), bufferIndex);

    if (const HRESULT result = copyCommandList_->Close(); FAILED(result))
    {
        return std::unexpected(describeFailure("ID3D12GraphicsCommandList::Close (VSR)", result));
    }

    if (shownFrame_->fence)
    {
        gpu_.queue->Wait(shownFrame_->fence.Get(), shownFrame_->fenceValue);
    }

    const std::array<ID3D12CommandList*, 1> copyLists{copyCommandList_.Get()};
    gpu_.queue->ExecuteCommandLists(static_cast<UINT>(copyLists.size()), copyLists.data());

    const UINT64 inputReady = videoSuperResolution_->inputReadyValue();
    gpu_.queue->Signal(videoSuperResolution_->fence(), inputReady);
    copyAllocatorFenceValues_.at(bufferIndex) = ++lastSignaledFence_;
    gpu_.queue->Signal(fence_.Get(), lastSignaledFence_);

    const auto outputReady = videoSuperResolution_->blit(bufferIndex, inputReady);

    if (!outputReady)
    {
        if (preparedSuperResolution_)
        {
            videoSuperResolutionFailed_ = true;
        }

        if (preparedTrueHdr_)
        {
            rtxVideoHdrFailed_ = true;
        }

        redrawRequested_.store(true);
        SetEvent(wakeEvent_.get());
        spdlog::error("The video processor stopped on this frame; the shader path draws the next "
                      "one: {}",
                      outputReady.error());
        return false;
    }

    auto& allocator = allocators_.at(bufferIndex);
    allocator->Reset();
    commandList_->Reset(allocator.Get(), nullptr);

    // The processor's work happened between the two lists: these stamps close
    // the stages after the queue has waited for it.
    if (preparedSuperResolution_)
    {
        timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::VideoSuperResolution);
    }

    if (preparedTrueHdr_)
    {
        timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::RtxVideoHdr);
    }

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::ColorConversion);

    // Optical flow reads a picture the presenter makes at the source size; the
    // video processor's output is at the fit size, in its own format.
    if (plan.writeFlowPicture && shownFrame_)
    {
        presenter_.recordConversion(*commandList_.Get(), bufferIndex, *shownFrame_);
        presenter_.recordFlowPicture(*commandList_.Get(), bufferIndex, flowToneMap());
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::ColorConversion);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Cadence);

    if (plan.cadence && cadenceMetric_)
    {
        cadenceSignal_ =
            cadenceMetric_->record(*commandList_.Get(), *shownFrame_, *plan.previous);
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Cadence);

    if (!preparedTrueHdr_)
    {
        timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::RtxVideoHdr);
        timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::RtxVideoHdr);
    }

    if (!preparedSuperResolution_)
    {
        timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::VideoSuperResolution);
        timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::VideoSuperResolution);
    }

    // DLSS doesn't run on the video processor's path; the stage still needs
    // its stamps for the frame's times to be read back.
    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::DlssSuperResolution);
    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::DlssSuperResolution);

    ID3D12Resource* const output = videoSuperResolution_->output(bufferIndex);
    const auto toShader = transition(output, D3D12_RESOURCE_STATE_COMMON,
                                     D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList_->ResourceBarrier(1, &toShader);

    const D3D12_CPU_DESCRIPTOR_HANDLE targetView = target.view;

    const auto toRenderTarget =
        transition(target.resource, target.before, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &toRenderTarget);
    commandList_->ClearRenderTargetView(targetView, kBlack.data(), 0, nullptr);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Scaling);
    const ToneMapParameters toneMap{
        .enabled = presentation_.toneMap,
        .contentPeakNits = presentation_.contentPeakNits,
        .sdrWhiteNits = presentation_.sdrWhiteNits,
    };
    presenter_.recordScaling(*commandList_.Get(), bufferIndex, targetView, swapChain_.size(),
                              picture, ScaledPicture::Upscaled, toneMap);
    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Scaling);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    if (overlay_ != nullptr)
    {
        overlay_->render(*commandList_.Get(), targetView, overlayStats(), gamepadState());
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    const auto pictureToCommon = transition(output, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                            D3D12_RESOURCE_STATE_COMMON);
    commandList_->ResourceBarrier(1, &pictureToCommon);

    const auto toNext = transition(target.resource, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                   target.after);
    commandList_->ResourceBarrier(1, &toNext);

    if (target.resource == realPicture_.Get())
    {
        realPictureState_ = target.after;
    }

    timer_->endFrame(*commandList_.Get(), bufferIndex);

    if (const HRESULT result = commandList_->Close(); FAILED(result))
    {
        return std::unexpected(describeFailure("ID3D12GraphicsCommandList::Close (VSR)", result));
    }

    // The draw reads the processor's output only after its signal; the flow
    // reads the converted picture after the list that wrote it.
    gpu_.queue->Wait(videoSuperResolution_->fence(), *outputReady);

    const std::array<ID3D12CommandList*, 1> lists{commandList_.Get()};
    gpu_.queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());
    signalCadence();
    return true;
}

void FrameRenderer::recordInputCopy(ID3D12GraphicsCommandList& commandList, UINT bufferIndex)
{
    ID3D12Resource* const input = videoSuperResolution_->input(bufferIndex);
    const D3D12_RESOURCE_DESC source = shownFrame_->texture->GetDesc();
    // A subresource is (plane, mip, slice); the decoder's textures have one
    // mip and the visible picture sits in `arraySlice`.
    const UINT subresourcesPerPlane =
        static_cast<UINT>(source.MipLevels) * source.DepthOrArraySize;

    const auto toCopy = transition(input, D3D12_RESOURCE_STATE_COMMON,
                                   D3D12_RESOURCE_STATE_COPY_DEST);
    commandList.ResourceBarrier(1, &toCopy);

    // NV12 and P010 hold two planes, and D3D12 copies one plane per region:
    // luma at full size and interleaved chroma at half.
    for (UINT plane = 0; plane < 2; ++plane)
    {
        const UINT divisor = plane == 0 ? 1 : 2;

        D3D12_TEXTURE_COPY_LOCATION planeSource{
            .pResource = shownFrame_->texture.Get(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = plane * subresourcesPerPlane + shownFrame_->arraySlice,
        };
        D3D12_TEXTURE_COPY_LOCATION planeDestination{
            .pResource = input,
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = plane,
        };
        const D3D12_BOX box{
            .left = 0,
            .top = 0,
            .front = 0,
            .right = shownFrame_->width / divisor,
            .bottom = shownFrame_->height / divisor,
            .back = 1,
        };

        commandList.CopyTextureRegion(&planeDestination, 0, 0, 0, &planeSource, &box);
    }

    const auto toCommon = transition(input, D3D12_RESOURCE_STATE_COPY_DEST,
                                     D3D12_RESOURCE_STATE_COMMON);
    commandList.ResourceBarrier(1, &toCommon);
}

OverlayStats FrameRenderer::overlayStats()
{
    OverlayStats stats;

    if (metrics_ != nullptr)
    {
        stats.video = metrics_->snapshot(core::StreamMetrics::Clock::now());
    }

    stats.outputWidth = swapChain_.size().width;
    stats.outputHeight = swapChain_.size().height;
    stats.hdrStream = presentation_.hdrStream;
    stats.hdrSwapChain = presentation_.hdrSwapChain;
    stats.rtxVideoHdr = presentation_.rtxVideoHdr;

    if (lastGpuTimes_)
    {
        stats.gpuTimingsValid = true;
        stats.stageMilliseconds = lastGpuTimes_->stages;
        stats.totalMilliseconds = lastGpuTimes_->total;
        stats.waitMilliseconds = lastGpuTimes_->wait;
    }

    if (lastFlowMilliseconds_)
    {
        stats.opticalFlowTimingValid = true;
        stats.opticalFlowMilliseconds = *lastFlowMilliseconds_;
    }

    stats.opticalFlow = opticalFlowState();
    stats.dlss = dlssState();
    stats.upscaling = requestedUpscalingApplied_;
    stats.frameGeneration = requestedFrameGenerationApplied_;

    if (const auto& cadence = cadence_.estimate())
    {
        stats.cadenceValid = true;
        stats.cadenceFramesPerSecond = cadence->framesPerSecond;
        stats.cadenceRepeatedFrames = static_cast<std::uint32_t>(cadence->repeatedFrames);
        stats.cadenceWindowFrames = static_cast<std::uint32_t>(cadence->windowFrames);
    }

    ReflexOverlayStats& reflex = stats.reflex;

    if (!reflex_.available())
    {
        reflex.state = ReflexOverlayStats::State::Unavailable;
        reflex.reason = reflexUnavailableReason(reflexReason_);
        return stats;
    }

    reflex.mode = reflexModeLabel(reflex_.mode());
    reflex.lowLatencyEffective = reflex_.sleepStatus().lowLatency;
    reflex.driverSeesSleep = reflex_.sleepStatus().driverSeesSleep;
    reflex.sleepMeanMs = reflex_.meanSleepMilliseconds();
    reflex.sleepMaxMs = reflex_.maxSleepMilliseconds();

    const ReflexLatency& latency = reflex_.latency();
    reflex.latencyValid = latency.valid;
    reflex.latencyTotalMs = latency.totalMilliseconds;
    reflex.latencyGpuMs = latency.gpuMilliseconds;
    reflex.latencyRenderMs = latency.renderSubmitMilliseconds;
    reflex.latencyPresentMs = latency.presentMilliseconds;
    reflex.latencyDriverMs = latency.driverMilliseconds;
    reflex.latencyQueueMs = latency.queueMilliseconds;

    if (reflex_.failed())
    {
        reflex.state = ReflexOverlayStats::State::Failed;
    }
    else if (reflex_.stalled())
    {
        reflex.state = ReflexOverlayStats::State::Stalled;
    }
    else if (latency.valid)
    {
        reflex.state = ReflexOverlayStats::State::Active;
    }
    else
    {
        reflex.state = ReflexOverlayStats::State::Waiting;
    }

    return stats;
}

std::uint32_t FrameRenderer::lastGpuWorkUs() const
{
    if (!lastGpuTimes_)
    {
        return 0;
    }

    constexpr double kMaxMicroseconds = 4'000'000.0;
    return static_cast<std::uint32_t>(
        std::min(lastGpuTimes_->total * 1000.0, kMaxMicroseconds));
}

std::string FrameRenderer::describePresentFailure(HRESULT result) const
{
    const bool deviceLost =
        result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET;

    if (!deviceLost)
    {
        return describeFailure("Present", result);
    }

    const auto reason = static_cast<unsigned long>(gpu_.device->GetDeviceRemovedReason());
    return std::format("{} (the GPU was removed, reason 0x{:08X})",
                       describeFailure("Present", result), reason);
}

void FrameRenderer::reportStatistics()
{
    const auto summary = presented_.takeIfDue(std::chrono::steady_clock::now());

    if (!summary)
    {
        return;
    }

    const std::uint64_t dropped = std::exchange(droppedFrames_, 0);

    if (summary->count == 0 && dropped == 0)
    {
        return;
    }

    spdlog::info("Presented {:.1f} fps, {:.1f} ms from frame ready to present on average ({:.1f} "
                 "ms at most), {} frames replaced before being shown",
                 summary->perSecond, summary->averageMilliseconds, summary->maxMilliseconds,
                 dropped);

    if (metrics_ != nullptr)
    {
        const core::StreamMetricsSnapshot video =
            metrics_->snapshot(core::StreamMetrics::Clock::now());

        spdlog::info("Rates: source {:.1f} fps, game {:.1f} fps, generated {:.1f} fps, displayed "
                     "{:.1f} fps; real presents {:.1f} fps, repeated {:.1f} fps",
                     video.sourceFps, video.gameFps, video.generatedFps, video.displayedFps,
                     video.realPresentedFps, video.repeatedPresentedFps);
    }

    if (lastGpuTimes_)
    {
        std::string stages;

        for (std::size_t stage = 0; stage < kEffectStageCount; ++stage)
        {
            stages += std::format("{} {:.2f} ms, ", stageName(static_cast<EffectStage>(stage)),
                                  lastGpuTimes_->stages.at(stage));
        }

        const std::string flow = lastFlowMilliseconds_
                                     ? std::format("{:.2f} ms", *lastFlowMilliseconds_)
                                     : "n/a";

        spdlog::info("GPU: {}total {:.2f} ms (stage work, no waits), wait {:.2f} ms; optical "
                     "flow engine {} (own engine, not in the total)",
                     stages, lastGpuTimes_->total, lastGpuTimes_->wait, flow);
    }

    if (const auto& cadence = cadence_.estimate())
    {
        spdlog::info("Cadence: {:.1f} fps, {} of the last {} source frames repeated",
                     cadence->framesPerSecond, cadence->repeatedFrames, cadence->windowFrames);
    }

    if (requestedFrameGenerationApplied_ == core::FrameGenerationMode::X2)
    {
        if (frameGenerationFailed_)
        {
            spdlog::info("Frame Generation: unavailable for this stream ({})",
                         frameGenerationFailure_);
        }
        else if (frameGeneration_)
        {
            spdlog::info("Frame Generation: {} generated pictures presented, {} evaluations",
                         generatedFrames_, frameGeneration_->evaluations());
        }
    }

    reflex_.logSummary();
}

void FrameRenderer::waitForFence(UINT64 value)
{
    if (fence_->GetCompletedValue() >= value)
    {
        return;
    }

    fence_->SetEventOnCompletion(value, fenceEvent_.get());
    WaitForSingleObject(fenceEvent_.get(), INFINITE);
}

void FrameRenderer::waitForGpu()
{
    if (!fence_ || !fenceEvent_)
    {
        return;
    }

    gpu_.queue->Signal(fence_.Get(), ++lastSignaledFence_);
    waitForFence(lastSignaledFence_);
}

} // namespace mfplay::render
