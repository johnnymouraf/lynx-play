#include "render/FrameRenderer.h"

#include "core/Latency.h"

#include <algorithm>
#include <format>
#include <span>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

namespace lynx::render
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
    requestedHdr_.store(options_.hdr);
    requestedHdrApplied_ = options_.hdr;

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

    reflex_.shutdown();
    waitForGpu();
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

void FrameRenderer::setHdrMode(core::HdrMode mode)
{
    requestedHdr_.store(mode);
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

    for (auto& allocator : flowAllocators_)
    {
        if (const HRESULT result = gpu_.device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
            FAILED(result))
        {
            return std::unexpected(describeFailure("CreateCommandAllocator (flow)", result));
        }
    }

    if (const HRESULT result = gpu_.device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, flowAllocators_[0].Get(), nullptr,
            IID_PPV_ARGS(&flowCommandList_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommandList (flow)", result));
    }

    flowCommandList_->Close();

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

    fenceEvent_.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    wakeEvent_.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));

    if (!fenceEvent_ || !wakeEvent_)
    {
        return std::unexpected(describeFailure("CreateEventW", HRESULT_FROM_WIN32(GetLastError())));
    }

    return {};
}

void FrameRenderer::run(const std::stop_token& stop)
{
    SetThreadDescription(GetCurrentThread(), L"Lynx frame renderer");
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
        applyRequestedHdr();
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

    std::optional<VideoFrame> newest = mailbox_->take();
    const bool redraw = redrawRequested_.exchange(false);

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
            dlssResetPending_ = true;
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
        }
    }

    // A buffer's command allocator can be reset only once the GPU has finished
    // the frame that last used it; so can the picture that frame read.
    const UINT bufferIndex = swapChain_.currentIndex();
    waitForFence(allocatorFenceValues_.at(bufferIndex));
    waitForFence(copyAllocatorFenceValues_.at(bufferIndex));
    waitForFence(flowAllocatorFenceValues_.at(bufferIndex));
    framesInFlight_.at(bufferIndex).reset();

    if (auto times = timer_->take(bufferIndex); times && bufferShowedNewFrame_.at(bufferIndex))
    {
        lastGpuTimes_ = *times;
    }

    const bool flowReady = prepareOpticalFlow();

    const UINT previousIndex =
        (bufferIndex + SwapChain::kBufferCount - 1) % SwapChain::kBufferCount;
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

    const bool marked = reflexFrameId_.has_value();
    const std::uint64_t reflexFrameId = reflexFrameId_.value_or(0);
    bool flowConsumed = false;
    dlssActive_ = false;

    if (marked)
    {
        reflex_.mark(reflexFrameId, LatencyMarker::SimulationStart);
        reflex_.mark(reflexFrameId, LatencyMarker::SimulationEnd);
        reflex_.mark(reflexFrameId, LatencyMarker::RenderSubmitStart);
    }

    if (prepareDlssSuperResolution(bufferIndex, plan.opticalFlow))
    {
        const auto recorded = recordDlssSuperResolutionFrame(bufferIndex, plan, flowConsumed);

        if (!recorded)
        {
            return std::unexpected(recorded.error());
        }
    }
    else if (prepareVideoSuperResolution())
    {
        const auto recorded = recordVideoSuperResolutionFrame(bufferIndex, plan);

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
        auto& allocator = allocators_.at(bufferIndex);
        allocator->Reset();
        commandList_->Reset(allocator.Get(), nullptr);
        recordShaderFrame(bufferIndex, plan);

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

    std::uint64_t flowOutput = 0;
    bool flowSubmitted = false;

    if (plan.opticalFlow && !flowConsumed)
    {
        const std::uint64_t inputValue = opticalFlow_->nextInputValue();
        gpu_.queue->Signal(opticalFlow_->fence(), inputValue);

        const auto output = opticalFlow_->execute(bufferIndex, inputValue);

        if (output)
        {
            flowOutput = *output;
            flowSubmitted = true;
            flowSlotValid_.at(bufferIndex) = true;
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
        reflex_.mark(reflexFrameId, LatencyMarker::PresentStart);
    }

    const HRESULT presentResult = swapChain_.present(options_.timing);

    if (marked)
    {
        reflex_.mark(reflexFrameId, LatencyMarker::PresentEnd);
    }

    if (FAILED(presentResult))
    {
        return std::unexpected(describePresentFailure(presentResult));
    }

    if (plan.opticalFlow && !flowConsumed)
    {
        if (auto watched = recordFlowWatch(bufferIndex, flowSubmitted, flowOutput); !watched)
        {
            return watched;
        }
    }

    gpu_.queue->Signal(fence_.Get(), ++lastSignaledFence_);
    allocatorFenceValues_.at(bufferIndex) = lastSignaledFence_;
    flowAllocatorFenceValues_.at(bufferIndex) = lastSignaledFence_;
    bufferShowedNewFrame_.at(bufferIndex) = showsNewFrame;
    framesInFlight_.at(bufferIndex) = shownFrame_;

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

    if (metrics_ != nullptr)
    {
        metrics_->addPresented(now, interval, readyToPresent, showsNewFrame);
    }

    if (showsNewFrame)
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

        // The engine registered the old intermediate textures.
        opticalFlow_.reset();
        flowReferenceValid_ = false;
        flowSlotValid_.fill(false);
        dlssResetPending_ = true;

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

bool FrameRenderer::prepareVideoSuperResolution()
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
    spdlog::info("Optical flow: {}", requested ? "on" : "off");
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

    if (!ngxHost_)
    {
        auto host = NgxHost::create(*gpu_.device.Get());

        if (!host)
        {
            dlssFailed_ = true;
            noteDlssFallback(host.error());
            return false;
        }

        ngxHost_ = std::move(*host);
    }

    if (!dlssSuperResolution_)
    {
        auto created = DlssSuperResolution::create(*ngxHost_, *gpu_.queue.Get(),
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
    if (cadenceSignal_ == 0 || cadenceMetric_ == nullptr)
    {
        return;
    }

    gpu_.queue->Signal(cadenceMetric_->fence(), cadenceSignal_);
    cadenceSignal_ = 0;
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

        if (cadence_.observe(comparison) == CadenceDetector::Decision::SceneCut)
        {
            dlssResetPending_ = true;
            spdlog::debug("Cadence: scene cut, window restarted");
        }
    }
}

std::expected<void, std::string> FrameRenderer::recordFlowWatch(UINT bufferIndex,
                                                                bool waitForFlow,
                                                                std::uint64_t outputValue)
{
    auto& allocator = flowAllocators_.at(bufferIndex);
    allocator->Reset();
    flowCommandList_->Reset(allocator.Get(), nullptr);

    if (waitForFlow)
    {
        gpu_.queue->Wait(opticalFlow_->fence(), outputValue);
    }

    timer_->endStage(*flowCommandList_.Get(), bufferIndex, EffectStage::OpticalFlow);
    timer_->endFrame(*flowCommandList_.Get(), bufferIndex);

    if (const HRESULT result = flowCommandList_->Close(); FAILED(result))
    {
        return std::unexpected(describeFailure("ID3D12GraphicsCommandList::Close (flow)", result));
    }

    const std::array<ID3D12CommandList*, 1> lists{flowCommandList_.Get()};
    gpu_.queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());
    return {};
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

void FrameRenderer::recordShaderFrame(UINT bufferIndex, const FramePlan& plan)
{
    ID3D12Resource* const backBuffer = swapChain_.backBuffer(bufferIndex);
    const D3D12_CPU_DESCRIPTOR_HANDLE target = swapChain_.renderTargetView(bufferIndex);

    timer_->beginFrame(*commandList_.Get(), bufferIndex);

    // The shader path doesn't run the video processor; those stages still
    // need valid stamps for the frame's times to be read back.
    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::RtxVideoHdr);
    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::RtxVideoHdr);
    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::VideoSuperResolution);
    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::VideoSuperResolution);

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
        transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &toRenderTarget);
    commandList_->ClearRenderTargetView(target, kBlack.data(), 0, nullptr);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Scaling);

    if (shownFrame_)
    {
        const SurfaceSize picture{.width = shownFrame_->width, .height = shownFrame_->height};
        const ToneMapParameters toneMap{
            .enabled = presentation_.toneMap,
            .contentPeakNits = presentation_.contentPeakNits,
            .sdrWhiteNits = presentation_.sdrWhiteNits,
        };
        presenter_.recordScaling(*commandList_.Get(), bufferIndex, target, swapChain_.size(),
                                  picture, ScaledPicture::Converted, toneMap);
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Scaling);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    if (overlay_ != nullptr)
    {
        overlay_->render(*commandList_.Get(), target, overlayStats(), gamepadState());
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    const auto toPresent =
        transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList_->ResourceBarrier(1, &toPresent);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::OpticalFlow);

    if (!plan.opticalFlow)
    {
        timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::OpticalFlow);
        timer_->endFrame(*commandList_.Get(), bufferIndex);
    }
}

std::expected<void, std::string> FrameRenderer::recordDlssSuperResolutionFrame(
    UINT bufferIndex, const FramePlan& plan, bool& flowConsumed)
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
    timer_->beginStage(*copyCommandList_.Get(), bufferIndex, EffectStage::OpticalFlow);

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

    std::uint64_t flowOutput = 0;
    bool waitForFlow = false;
    bool flowFailed = false;

    if (plan.opticalFlow)
    {
        const std::uint64_t inputValue = opticalFlow_->nextInputValue();
        gpu_.queue->Signal(opticalFlow_->fence(), inputValue);

        const auto flowValue = opticalFlow_->execute(bufferIndex, inputValue);
        flowConsumed = true;

        if (flowValue)
        {
            flowOutput = *flowValue;
            waitForFlow = true;
            flowSlotValid_.at(bufferIndex) = true;
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
        gpu_.queue->Wait(opticalFlow_->fence(), flowOutput);
    }
    else if (const std::uint64_t lastFlow = opticalFlow_->lastOutputValue(); lastFlow != 0)
    {
        // A redraw reads this slot's last vectors; waiting on the engine's
        // newest value covers every flow it has written.
        gpu_.queue->Wait(opticalFlow_->fence(), lastFlow);
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::OpticalFlow);

    bool drewDlss = false;

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::DlssSuperResolution);

    if (!flowFailed)
    {
        ID3D12Resource* const color = presenter_.flowIntermediate(bufferIndex);
        ID3D12Resource* const flow = opticalFlow_->flow(bufferIndex);

        if (color != nullptr && flow != nullptr)
        {
            const bool reset = std::exchange(dlssResetPending_, false);

            if (auto recorded = dlssSuperResolution_->record(*commandList_.Get(), bufferIndex,
                                                             *color, *flow, reset);
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

    ID3D12Resource* const backBuffer = swapChain_.backBuffer(bufferIndex);
    const D3D12_CPU_DESCRIPTOR_HANDLE target = swapChain_.renderTargetView(bufferIndex);

    const auto toRenderTarget =
        transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &toRenderTarget);
    commandList_->ClearRenderTargetView(target, kBlack.data(), 0, nullptr);

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

    presenter_.recordScaling(*commandList_.Get(), bufferIndex, target, swapChain_.size(),
                              drewDlss ? output : picture,
                              drewDlss ? ScaledPicture::Upscaled : ScaledPicture::Converted,
                              drewDlss ? dlssToneMap : convertedToneMap);
    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Scaling);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    if (overlay_ != nullptr)
    {
        overlay_->render(*commandList_.Get(), target, overlayStats(), gamepadState());
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    const auto toPresent =
        transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList_->ResourceBarrier(1, &toPresent);

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
    UINT bufferIndex, const FramePlan& plan)
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

    ID3D12Resource* const output = videoSuperResolution_->output(bufferIndex);
    const auto toShader = transition(output, D3D12_RESOURCE_STATE_COMMON,
                                     D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList_->ResourceBarrier(1, &toShader);

    ID3D12Resource* const backBuffer = swapChain_.backBuffer(bufferIndex);
    const D3D12_CPU_DESCRIPTOR_HANDLE target = swapChain_.renderTargetView(bufferIndex);

    const auto toRenderTarget =
        transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &toRenderTarget);
    commandList_->ClearRenderTargetView(target, kBlack.data(), 0, nullptr);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Scaling);
    const ToneMapParameters toneMap{
        .enabled = presentation_.toneMap,
        .contentPeakNits = presentation_.contentPeakNits,
        .sdrWhiteNits = presentation_.sdrWhiteNits,
    };
    presenter_.recordScaling(*commandList_.Get(), bufferIndex, target, swapChain_.size(), picture,
                              ScaledPicture::Upscaled, toneMap);
    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Scaling);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    if (overlay_ != nullptr)
    {
        overlay_->render(*commandList_.Get(), target, overlayStats(), gamepadState());
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    const auto pictureToCommon = transition(output, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                            D3D12_RESOURCE_STATE_COMMON);
    commandList_->ResourceBarrier(1, &pictureToCommon);

    const auto toPresent = transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                      D3D12_RESOURCE_STATE_PRESENT);
    commandList_->ResourceBarrier(1, &toPresent);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::OpticalFlow);

    if (!plan.opticalFlow)
    {
        timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::OpticalFlow);
        timer_->endFrame(*commandList_.Get(), bufferIndex);
    }

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
    }

    stats.opticalFlow = opticalFlowState();
    stats.dlss = dlssState();

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

    if (lastGpuTimes_)
    {
        std::string stages;

        for (std::size_t stage = 0; stage < kEffectStageCount; ++stage)
        {
            stages += std::format("{} {:.2f} ms, ", stageName(static_cast<EffectStage>(stage)),
                                  lastGpuTimes_->stages.at(stage));
        }

        spdlog::info("GPU: {}total {:.2f} ms", stages, lastGpuTimes_->total);
    }

    if (const auto& cadence = cadence_.estimate())
    {
        spdlog::info("Cadence: {:.1f} fps, {} of the last {} source frames repeated",
                     cadence->framesPerSecond, cadence->repeatedFrames, cadence->windowFrames);
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

} // namespace lynx::render
