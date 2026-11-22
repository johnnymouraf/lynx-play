#include "render/FrameRenderer.h"

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

    auto presenter =
        FramePresenter::create(*gpu.device.Get(), SwapChain::kBufferCount, SwapChain::kFormat);

    if (!presenter)
    {
        return std::unexpected(presenter.error());
    }

    // The constructor is private, out of make_unique's reach.
    std::unique_ptr<FrameRenderer> renderer(new FrameRenderer(
        gpu, window, std::move(*swapChain), std::move(*presenter), std::move(mailbox),
        std::move(options)));

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
                             FramePresenter presenter, std::shared_ptr<FrameMailbox> mailbox,
                             Options options)
    : gpu_(std::move(gpu)),
      window_(window),
      swapChain_(std::move(swapChain)),
      presenter_(std::move(presenter)),
      mailbox_(std::move(mailbox)),
      options_(std::move(options)),
      requestedSize_(swapChain_.size()),
      presented_(std::chrono::steady_clock::now(), kStatisticsPeriod)
{
    requestedReflex_.store(options_.reflex);
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

    const std::span<const std::uint8_t> font =
        options_.overlayFont ? std::span<const std::uint8_t>(*options_.overlayFont)
                             : std::span<const std::uint8_t>{};

    std::unique_ptr<Overlay> overlay;
    auto created = Overlay::create(*gpu_.device.Get(), *gpu_.queue.Get(), window_,
                                   SwapChain::kBufferCount, SwapChain::kFormat, font);

    if (!created)
    {
        spdlog::warn("The overlay is unavailable: {}", created.error());
    }
    else
    {
        overlay = std::move(*created);
    }

    const std::stop_callback wakeOnStop(stop, [this] { SetEvent(wakeEvent_.get()); });
    const std::array events{mailbox_->frameReadyEvent(), wakeEvent_.get()};

    while (true)
    {
        WaitForMultipleObjects(static_cast<DWORD>(events.size()), events.data(), FALSE, INFINITE);

        if (stop.stop_requested())
        {
            return;
        }

        applyRequestedReflex();

        if (auto frame = renderFrame(overlay.get()); !frame)
        {
            spdlog::error("Rendering stopped: {}", frame.error());

            if (options_.onFailure)
            {
                options_.onFailure();
            }

            return;
        }

        reportStatistics();
    }
}

std::expected<void, std::string> FrameRenderer::renderFrame(Overlay* overlay)
{
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
    }

    if (auto resized = applyRequestedSize(); !resized)
    {
        return resized;
    }

    if (shownFrame_)
    {
        const SurfaceSize picture{.width = shownFrame_->width, .height = shownFrame_->height};

        if (presenter_.needsPicture(picture))
        {
            waitForGpu();

            if (auto resized = presenter_.resizePicture(picture); !resized)
            {
                return resized;
            }
        }
    }

    // A buffer's command allocator can be reset only once the GPU has finished
    // the frame that last used it; so can the picture that frame read.
    const UINT bufferIndex = swapChain_.currentIndex();
    waitForFence(allocatorFenceValues_.at(bufferIndex));
    framesInFlight_.at(bufferIndex).reset();

    if (auto times = timer_->take(bufferIndex))
    {
        lastGpuTimes_ = *times;
    }

    const bool marked = reflexFrameId_.has_value();
    const std::uint64_t reflexFrameId = reflexFrameId_.value_or(0);

    if (marked)
    {
        reflex_.mark(reflexFrameId, LatencyMarker::SimulationStart);
    }

    auto& allocator = allocators_.at(bufferIndex);
    allocator->Reset();
    commandList_->Reset(allocator.Get(), nullptr);

    if (marked)
    {
        reflex_.mark(reflexFrameId, LatencyMarker::SimulationEnd);
        reflex_.mark(reflexFrameId, LatencyMarker::RenderSubmitStart);
    }

    recordFrame(bufferIndex, overlay);
    commandList_->Close();

    if (shownFrame_ && shownFrame_->fence)
    {
        gpu_.queue->Wait(shownFrame_->fence.Get(), shownFrame_->fenceValue);
    }

    const std::array<ID3D12CommandList*, 1> lists{commandList_.Get()};
    gpu_.queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());

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

    gpu_.queue->Signal(fence_.Get(), ++lastSignaledFence_);
    allocatorFenceValues_.at(bufferIndex) = lastSignaledFence_;
    framesInFlight_.at(bufferIndex) = shownFrame_;

    if (showsNewFrame)
    {
        presented_.add(std::chrono::steady_clock::now() - shownFrame_->timestamp);
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

void FrameRenderer::recordFrame(UINT bufferIndex, Overlay* overlay)
{
    ID3D12Resource* const backBuffer = swapChain_.backBuffer(bufferIndex);
    const D3D12_CPU_DESCRIPTOR_HANDLE target = swapChain_.renderTargetView(bufferIndex);

    timer_->beginFrame(*commandList_.Get(), bufferIndex);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::ColorConversion);

    if (shownFrame_)
    {
        presenter_.recordConversion(*commandList_.Get(), bufferIndex, *shownFrame_);
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::ColorConversion);

    const auto toRenderTarget =
        transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &toRenderTarget);
    commandList_->ClearRenderTargetView(target, kBlack.data(), 0, nullptr);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Scaling);

    if (shownFrame_)
    {
        presenter_.recordScaling(*commandList_.Get(), bufferIndex, target, swapChain_.size());
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Scaling);

    timer_->beginStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    if (overlay != nullptr)
    {
        overlay->render(*commandList_.Get(), target, overlayStats());
    }

    timer_->endStage(*commandList_.Get(), bufferIndex, EffectStage::Overlay);

    const auto toPresent =
        transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList_->ResourceBarrier(1, &toPresent);

    timer_->endFrame(*commandList_.Get(), bufferIndex);
}

OverlayStats FrameRenderer::overlayStats() const
{
    OverlayStats stats;

    if (lastGpuTimes_)
    {
        stats.gpuTimingsValid = true;
        stats.stageMilliseconds = lastGpuTimes_->stages;
        stats.totalMilliseconds = lastGpuTimes_->total;
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

    const std::uint64_t dropped = mailbox_->takeDroppedCount();

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
        spdlog::info("GPU: {} {:.2f} ms, {} {:.2f} ms, {} {:.2f} ms, total {:.2f} ms",
                     stageName(EffectStage::ColorConversion), lastGpuTimes_->stages.at(0),
                     stageName(EffectStage::Scaling), lastGpuTimes_->stages.at(1),
                     stageName(EffectStage::Overlay), lastGpuTimes_->stages.at(2),
                     lastGpuTimes_->total);
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
