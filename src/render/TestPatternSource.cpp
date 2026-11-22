#include "render/TestPatternSource.h"

#include "core/FramePacer.h"
#include "render/SwapChain.h"
#include "render/TestPattern.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <span>

#include <spdlog/spdlog.h>

namespace lynx::render
{

namespace
{

using core::describeFailure;

constexpr SurfaceSize kPictureSize{.width = 1920, .height = 1080};
constexpr double kFramesPerSecond = 60.0;
constexpr UINT kPlaneCount = 2;
// The renderer keeps a frame per back buffer and the one on screen, and the
// mailbox holds one more.
constexpr std::size_t kSlotCount = SwapChain::kBufferCount + 3;

} // namespace

// A frame's picture: its texture, and the upload buffer it is written into
// before being copied there.
struct TestPatternSource::Slot
{
    ComPtr<ID3D12Resource> texture;
    ComPtr<ID3D12Resource> upload;
    std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, kPlaneCount> planes{};
    std::span<std::uint8_t> mapped;
    ComPtr<ID3D12CommandAllocator> allocator;
    // The fence value of the last copy out of `upload`.
    std::uint64_t copiedAt = 0;
    // Set while the renderer holds the frame.
    std::atomic<bool> leased = false;
};

std::expected<std::unique_ptr<TestPatternSource>, std::string> TestPatternSource::start(
    const GpuDevice& gpu, std::shared_ptr<FrameMailbox> mailbox)
{
    // The constructor is private, out of make_unique's reach.
    std::unique_ptr<TestPatternSource> source(
        new TestPatternSource(gpu.device, std::move(mailbox)));

    if (auto resources = source->createResources(); !resources)
    {
        return std::unexpected(resources.error());
    }

    TestPatternSource* const self = source.get();
    source->thread_ = std::jthread([self](const std::stop_token& stop) { self->run(stop); });

    return source;
}

TestPatternSource::TestPatternSource(ComPtr<ID3D12Device> device,
                                     std::shared_ptr<FrameMailbox> mailbox)
    : device_(std::move(device)), mailbox_(std::move(mailbox))
{
}

TestPatternSource::~TestPatternSource()
{
    if (thread_.joinable())
    {
        thread_.request_stop();
        thread_.join();
    }

    if (fence_ && fenceEvent_)
    {
        waitForCopy(lastSignaled_);
    }
}

std::expected<void, std::string> TestPatternSource::createResources()
{
    const D3D12_COMMAND_QUEUE_DESC queueDescription{.Type = D3D12_COMMAND_LIST_TYPE_COPY};

    if (const HRESULT result =
            device_->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&copyQueue_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommandQueue", result));
    }

    for (std::size_t index = 0; index < kSlotCount; ++index)
    {
        auto slot = createSlot();

        if (!slot)
        {
            return std::unexpected(slot.error());
        }

        slots_.push_back(std::move(*slot));
    }

    if (const HRESULT result = device_->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_COPY, slots_.front()->allocator.Get(), nullptr,
            IID_PPV_ARGS(&commandList_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommandList", result));
    }

    commandList_->Close();

    if (const HRESULT result =
            device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
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

std::expected<std::shared_ptr<TestPatternSource::Slot>, std::string> TestPatternSource::createSlot()
{
    auto slot = std::make_shared<Slot>();

    const D3D12_RESOURCE_DESC textureDescription{
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = kPictureSize.width,
        .Height = kPictureSize.height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_NV12,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
    };
    const D3D12_HEAP_PROPERTIES defaultHeap{.Type = D3D12_HEAP_TYPE_DEFAULT};

    if (const HRESULT result = device_->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDescription, D3D12_RESOURCE_STATE_COMMON,
            nullptr, IID_PPV_ARGS(&slot->texture));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommittedResource (NV12 texture)", result));
    }

    UINT64 uploadSize = 0;
    device_->GetCopyableFootprints(&textureDescription, 0, kPlaneCount, 0, slot->planes.data(),
                                   nullptr, nullptr, &uploadSize);

    const D3D12_RESOURCE_DESC bufferDescription{
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = uploadSize,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
    };
    const D3D12_HEAP_PROPERTIES uploadHeap{.Type = D3D12_HEAP_TYPE_UPLOAD};

    if (const HRESULT result = device_->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDescription,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&slot->upload));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommittedResource (upload)", result));
    }

    void* mapped = nullptr;
    const D3D12_RANGE noReads{.Begin = 0, .End = 0};

    if (const HRESULT result = slot->upload->Map(0, &noReads, &mapped); FAILED(result))
    {
        return std::unexpected(describeFailure("ID3D12Resource::Map", result));
    }

    slot->mapped = {static_cast<std::uint8_t*>(mapped), static_cast<std::size_t>(uploadSize)};

    if (const HRESULT result = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,
                                                               IID_PPV_ARGS(&slot->allocator));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommandAllocator", result));
    }

    return slot;
}

void TestPatternSource::run(const std::stop_token& stop)
{
    SetThreadDescription(GetCurrentThread(), L"Lynx test pattern");

    auto pacer = core::FramePacer::create(kFramesPerSecond);

    if (!pacer)
    {
        spdlog::error("The test pattern can't start: {}", pacer.error());
        return;
    }

    const auto startTime = std::chrono::steady_clock::now();

    while (pacer->waitForNextTick(stop))
    {
        const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - startTime;

        if (auto drawn = drawFrame(elapsed.count()); !drawn)
        {
            spdlog::error("The test pattern stopped: {}", drawn.error());
            return;
        }
    }
}

std::expected<void, std::string> TestPatternSource::drawFrame(double seconds)
{
    const std::shared_ptr<Slot> slot = freeSlot();

    // The renderer holds every frame: skip this one rather than wait.
    if (!slot)
    {
        return {};
    }

    waitForCopy(slot->copiedAt);

    const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& lumaLayout = slot->planes[0];
    const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& chromaLayout = slot->planes[1];
    writeNv12(testPatternAt(seconds, kPictureSize), kPictureSize,
              {.bytes = slot->mapped.subspan(lumaLayout.Offset),
               .pitch = lumaLayout.Footprint.RowPitch},
              {.bytes = slot->mapped.subspan(chromaLayout.Offset),
               .pitch = chromaLayout.Footprint.RowPitch});

    slot->allocator->Reset();
    commandList_->Reset(slot->allocator.Get(), nullptr);

    for (UINT plane = 0; plane < kPlaneCount; ++plane)
    {
        D3D12_TEXTURE_COPY_LOCATION destination{
            .pResource = slot->texture.Get(),
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        };
        destination.SubresourceIndex = plane;

        D3D12_TEXTURE_COPY_LOCATION source{
            .pResource = slot->upload.Get(),
            .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
        };
        source.PlacedFootprint = slot->planes.at(plane);

        commandList_->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    }

    if (const HRESULT result = commandList_->Close(); FAILED(result))
    {
        return std::unexpected(describeFailure("ID3D12GraphicsCommandList::Close", result));
    }

    const std::array<ID3D12CommandList*, 1> lists{commandList_.Get()};
    copyQueue_->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());
    copyQueue_->Signal(fence_.Get(), ++lastSignaled_);
    slot->copiedAt = lastSignaled_;
    slot->leased.store(true);

    mailbox_->publish({
        .texture = slot->texture,
        .format = PixelFormat::Nv12,
        .matrix = ColorMatrix::Bt709,
        .range = ColorRange::Limited,
        .width = kPictureSize.width,
        .height = kPictureSize.height,
        .fence = fence_,
        .fenceValue = lastSignaled_,
        .timestamp = std::chrono::steady_clock::now(),
        .lease = std::shared_ptr<const void>(nullptr,
                                             [slot](const void*) { slot->leased.store(false); }),
    });

    return {};
}

std::shared_ptr<TestPatternSource::Slot> TestPatternSource::freeSlot()
{
    const auto isFree = [](const std::shared_ptr<Slot>& slot) { return !slot->leased.load(); };
    const auto found = std::ranges::find_if(slots_, isFree);

    return found != slots_.end() ? *found : nullptr;
}

void TestPatternSource::waitForCopy(std::uint64_t value)
{
    if (fence_->GetCompletedValue() >= value)
    {
        return;
    }

    fence_->SetEventOnCompletion(value, fenceEvent_.get());
    WaitForSingleObject(fenceEvent_.get(), INFINITE);
}

} // namespace lynx::render
