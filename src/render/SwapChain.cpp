#include "render/SwapChain.h"

#include <spdlog/spdlog.h>

namespace lynx::render
{

namespace
{

using core::describeFailure;

constexpr UINT kMaxQueuedFrames = 1;

} // namespace

std::expected<SwapChain, std::string> SwapChain::create(const GpuDevice& gpu, HWND window,
                                                        SurfaceSize size, DXGI_FORMAT format)
{
    SwapChain chain;
    chain.device_ = gpu.device;
    chain.factory_ = gpu.factory;
    chain.queue_ = gpu.queue;
    chain.window_ = window;
    chain.tearing_ = gpu.supportsTearing;
    chain.size_ = size;

    if (auto created = chain.createChain(size, format); !created)
    {
        return std::unexpected(created.error());
    }

    const D3D12_DESCRIPTOR_HEAP_DESC heapDescription{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = kBufferCount,
    };

    if (const HRESULT result =
            gpu.device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&chain.rtvHeap_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateDescriptorHeap", result));
    }

    chain.rtvDescriptorSize_ =
        gpu.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    if (auto targets = chain.createRenderTargets(); !targets)
    {
        return std::unexpected(targets.error());
    }

    return chain;
}

std::expected<void, std::string> SwapChain::resize(SurfaceSize size)
{
    return resize(size, format_);
}

std::expected<void, std::string> SwapChain::resize(SurfaceSize size, DXGI_FORMAT format)
{
    for (auto& buffer : buffers_)
    {
        buffer.Reset();
    }

    if (format != format_)
    {
        swapChain_.Reset();

        if (auto created = createChain(size, format); !created)
        {
            return created;
        }
    }
    else
    {
        if (const HRESULT result = swapChain_->ResizeBuffers(kBufferCount, size.width,
                                                             size.height, format_, flags());
            FAILED(result))
        {
            return std::unexpected(describeFailure("ResizeBuffers", result));
        }

        size_ = size;

        if (auto applied = applyColorSpace(); !applied)
        {
            return applied;
        }
    }

    return createRenderTargets();
}

bool SwapChain::waitForFrameSlot(std::chrono::milliseconds timeout) const
{
    const auto milliseconds = static_cast<DWORD>(timeout.count());

    return WaitForSingleObjectEx(frameLatencyWaitable_.get(), milliseconds, TRUE) ==
           WAIT_OBJECT_0;
}

HRESULT SwapChain::present(PresentTiming timing)
{
    if (timing == PresentTiming::VSync)
    {
        return swapChain_->Present(1, 0);
    }

    const UINT presentFlags = tearing_ ? DXGI_PRESENT_ALLOW_TEARING : 0;
    return swapChain_->Present(0, presentFlags);
}

UINT SwapChain::currentIndex() const
{
    return swapChain_->GetCurrentBackBufferIndex();
}

ID3D12Resource* SwapChain::backBuffer(UINT index) const
{
    return buffers_.at(index).Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::renderTargetView(UINT index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * rtvDescriptorSize_;

    return handle;
}

SurfaceSize SwapChain::size() const
{
    return size_;
}

DXGI_FORMAT SwapChain::format() const
{
    return format_;
}

std::expected<void, std::string> SwapChain::createChain(SurfaceSize size, DXGI_FORMAT format)
{
    const DXGI_SWAP_CHAIN_DESC1 description{
        .Width = size.width,
        .Height = size.height,
        .Format = format,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = kBufferCount,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
        .Flags = flags(),
    };

    ComPtr<IDXGISwapChain1> swapChain1;

    if (const HRESULT result = factory_->CreateSwapChainForHwnd(
            queue_.Get(), window_, &description, nullptr, nullptr, &swapChain1);
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateSwapChainForHwnd", result));
    }

    // Fullscreen is borderless and handled by the window; DXGI's exclusive mode
    // would fight it.
    if (const HRESULT result = factory_->MakeWindowAssociation(window_, DXGI_MWA_NO_ALT_ENTER);
        FAILED(result))
    {
        spdlog::warn("{}", describeFailure("MakeWindowAssociation", result));
    }

    if (const HRESULT result = swapChain1.As(&swapChain_); FAILED(result))
    {
        return std::unexpected(describeFailure("IDXGISwapChain3 query", result));
    }

    if (const HRESULT result = swapChain_->SetMaximumFrameLatency(kMaxQueuedFrames);
        FAILED(result))
    {
        return std::unexpected(describeFailure("SetMaximumFrameLatency", result));
    }

    spdlog::info("Presentation: frame queue depth {} (one frame at most waits for the display)",
                 kMaxQueuedFrames);

    frameLatencyWaitable_.reset(swapChain_->GetFrameLatencyWaitableObject());
    size_ = size;
    format_ = format;

    return applyColorSpace();
}

std::expected<void, std::string> SwapChain::applyColorSpace()
{
    const bool hdr = format_ == kHdrFormat;
    const DXGI_COLOR_SPACE_TYPE colorSpace =
        hdr ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
            : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

    UINT support = 0;

    if (const HRESULT result = swapChain_->CheckColorSpaceSupport(colorSpace, &support);
        FAILED(result))
    {
        spdlog::warn("{}", describeFailure("CheckColorSpaceSupport", result));
    }
    else if ((support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == 0)
    {
        spdlog::warn("The swap chain doesn't report color space {} as presentable; setting it "
                     "anyway",
                     static_cast<unsigned>(colorSpace));
    }

    if (const HRESULT result = swapChain_->SetColorSpace1(colorSpace); FAILED(result))
    {
        if (hdr)
        {
            return std::unexpected(describeFailure("SetColorSpace1", result));
        }

        // sRGB is the swap chain's default; a refusal here leaves it as it is.
        spdlog::warn("{}", describeFailure("SetColorSpace1 (SDR)", result));
    }

    if (hdr)
    {
        spdlog::info("Swap chain: HDR10 (R10G10B10A2_UNORM, RGB_FULL_G2084_NONE_P2020)");
    }
    else
    {
        spdlog::debug("Swap chain: SDR (R8G8B8A8_UNORM, RGB_FULL_G22_NONE_P709)");
    }

    return {};
}

std::expected<void, std::string> SwapChain::createRenderTargets()
{
    for (UINT index = 0; index < kBufferCount; ++index)
    {
        if (const HRESULT result = swapChain_->GetBuffer(index, IID_PPV_ARGS(&buffers_[index]));
            FAILED(result))
        {
            return std::unexpected(describeFailure("IDXGISwapChain::GetBuffer", result));
        }

        device_->CreateRenderTargetView(buffers_[index].Get(), nullptr, renderTargetView(index));
    }

    return {};
}

UINT SwapChain::flags() const
{
    UINT result = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    if (tearing_)
    {
        result |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    }

    return result;
}

} // namespace lynx::render
