#include "render/FrameRenderer.h"

#include "render/TestPattern.h"

#include <spdlog/spdlog.h>

namespace lynx::render
{

namespace
{

using core::describeFailure;

// Long enough to never fire while frames flow, short enough to notice a stop
// request promptly.
constexpr std::chrono::milliseconds kFrameSlotTimeout{100};

constexpr std::array<float, 4> kBarColor{1.0F, 1.0F, 1.0F, 1.0F};

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

} // namespace

std::expected<std::unique_ptr<FrameRenderer>, std::string> FrameRenderer::start(HWND window,
                                                                               SurfaceSize size)
{
    auto gpu = createGpuDevice();

    if (!gpu)
    {
        return std::unexpected(gpu.error());
    }

    auto swapChain = SwapChain::create(*gpu, window, size);

    if (!swapChain)
    {
        return std::unexpected(swapChain.error());
    }

    // The constructor is private, out of make_unique's reach.
    std::unique_ptr<FrameRenderer> renderer(
        new FrameRenderer(std::move(*gpu), std::move(*swapChain)));

    if (auto resources = renderer->createFrameResources(); !resources)
    {
        return std::unexpected(resources.error());
    }

    FrameRenderer* const self = renderer.get();
    renderer->thread_ = std::jthread([self](const std::stop_token& stop) { self->run(stop); });

    return renderer;
}

FrameRenderer::FrameRenderer(GpuDevice gpu, SwapChain swapChain)
    : gpu_(std::move(gpu)), swapChain_(std::move(swapChain)), requestedSize_(swapChain_.size())
{
}

FrameRenderer::~FrameRenderer()
{
    if (thread_.joinable())
    {
        thread_.request_stop();
        thread_.join();
    }

    waitForGpu();
}

void FrameRenderer::resize(SurfaceSize size)
{
    if (size.width == 0 || size.height == 0)
    {
        return;
    }

    requestedSize_.store(size);
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

    fenceEvent_.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));

    if (!fenceEvent_)
    {
        return std::unexpected(describeFailure("CreateEventW", HRESULT_FROM_WIN32(GetLastError())));
    }

    return {};
}

void FrameRenderer::run(const std::stop_token& stop)
{
    SetThreadDescription(GetCurrentThread(), L"Lynx frame renderer");

    while (!stop.stop_requested())
    {
        if (!swapChain_.waitForFrameSlot(kFrameSlotTimeout))
        {
            continue;
        }

        if (auto frame = renderFrame(); !frame)
        {
            spdlog::error("Rendering stopped: {}", frame.error());
            return;
        }
    }
}

std::expected<void, std::string> FrameRenderer::renderFrame()
{
    if (auto resized = applyRequestedSize(); !resized)
    {
        return resized;
    }

    // A buffer's command allocator can be reset only once the GPU has finished
    // the frame that last used it.
    const UINT bufferIndex = swapChain_.currentIndex();
    waitForFence(allocatorFenceValues_.at(bufferIndex));

    auto& allocator = allocators_.at(bufferIndex);
    allocator->Reset();
    commandList_->Reset(allocator.Get(), nullptr);
    recordTestPattern(bufferIndex);
    commandList_->Close();

    const std::array<ID3D12CommandList*, 1> lists{commandList_.Get()};
    gpu_.queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());

    if (const HRESULT result = swapChain_.present(PresentTiming::VSync); FAILED(result))
    {
        return std::unexpected(describeFailure("Present", result));
    }

    gpu_.queue->Signal(fence_.Get(), ++lastSignaledFence_);
    allocatorFenceValues_.at(bufferIndex) = lastSignaledFence_;

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

void FrameRenderer::waitForFence(UINT64 value)
{
    if (fence_->GetCompletedValue() >= value)
    {
        return;
    }

    fence_->SetEventOnCompletion(value, fenceEvent_.get());
    WaitForSingleObject(fenceEvent_.get(), INFINITE);
}

void FrameRenderer::recordTestPattern(UINT bufferIndex)
{
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count();
    const TestPattern pattern = testPatternAt(seconds, swapChain_.size());

    ID3D12Resource* const backBuffer = swapChain_.backBuffer(bufferIndex);
    const D3D12_CPU_DESCRIPTOR_HANDLE target = swapChain_.renderTargetView(bufferIndex);

    const auto toRenderTarget =
        transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &toRenderTarget);

    commandList_->ClearRenderTargetView(target, pattern.background.data(), 0, nullptr);
    commandList_->ClearRenderTargetView(target, kBarColor.data(), 1, &pattern.bar);

    const auto toPresent =
        transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList_->ResourceBarrier(1, &toPresent);
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
