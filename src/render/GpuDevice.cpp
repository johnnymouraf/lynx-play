#include "render/GpuDevice.h"

#include "core/Windows.h"

#include <spdlog/spdlog.h>

namespace lynx::render
{

namespace
{

using core::describeFailure;

constexpr INT kHighestGpuThreadPriority = 7;

std::expected<ComPtr<IDXGIFactory6>, std::string> createFactory()
{
    ComPtr<IDXGIFactory6> factory;

    if (const HRESULT result = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)); FAILED(result))
    {
        return std::unexpected(describeFailure("CreateDXGIFactory2", result));
    }

    return factory;
}

std::vector<ComPtr<IDXGIAdapter4>> enumerateAdapters(IDXGIFactory6& factory)
{
    std::vector<ComPtr<IDXGIAdapter4>> adapters;

    for (UINT index = 0;; ++index)
    {
        ComPtr<IDXGIAdapter4> adapter;
        const HRESULT result = factory.EnumAdapterByGpuPreference(
            index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));

        if (result == DXGI_ERROR_NOT_FOUND)
        {
            return adapters;
        }

        if (SUCCEEDED(result))
        {
            adapters.push_back(std::move(adapter));
        }
    }
}

AdapterInfo describe(IDXGIAdapter4& adapter)
{
    DXGI_ADAPTER_DESC3 description{};
    adapter.GetDesc3(&description);

    return {
        .name = core::toUtf8(description.Description),
        .vendorId = description.VendorId,
        .isSoftware = (description.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) != 0,
    };
}

std::vector<AdapterInfo> describeAll(const std::vector<ComPtr<IDXGIAdapter4>>& adapters)
{
    std::vector<AdapterInfo> infos;
    infos.reserve(adapters.size());

    for (const auto& adapter : adapters)
    {
        infos.push_back(describe(*adapter.Get()));
    }

    return infos;
}

bool supportsTearing(IDXGIFactory6& factory)
{
    BOOL allowed = FALSE;
    const HRESULT result = factory.CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                       &allowed, sizeof(allowed));

    return SUCCEEDED(result) && allowed == TRUE;
}

} // namespace

std::expected<std::vector<AdapterInfo>, std::string> listAdapters()
{
    const auto factory = createFactory();

    if (!factory)
    {
        return std::unexpected(factory.error());
    }

    return describeAll(enumerateAdapters(*factory->Get()));
}

std::expected<GpuDevice, std::string> createGpuDevice()
{
    auto factory = createFactory();

    if (!factory)
    {
        return std::unexpected(factory.error());
    }

    const auto adapters = enumerateAdapters(*factory->Get());
    const auto infos = describeAll(adapters);
    const auto chosen = selectAdapter(infos);

    if (!chosen)
    {
        return std::unexpected("no hardware GPU supports Direct3D 12");
    }

    GpuDevice gpu{
        .factory = *factory,
        .adapter = adapters[*chosen],
        .info = infos[*chosen],
        .supportsTearing = supportsTearing(*factory->Get()),
    };

    if (const HRESULT result = D3D12CreateDevice(gpu.adapter.Get(), D3D_FEATURE_LEVEL_12_0,
                                                 IID_PPV_ARGS(&gpu.device));
        FAILED(result))
    {
        return std::unexpected(describeFailure("D3D12CreateDevice on " + gpu.info.name, result));
    }

    const D3D12_COMMAND_QUEUE_DESC queueDescription{
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
    };

    if (const HRESULT result =
            gpu.device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&gpu.queue));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommandQueue", result));
    }

    spdlog::info("Rendering on {} (vendor 0x{:04X}), tearing {}", gpu.info.name,
                 gpu.info.vendorId, gpu.supportsTearing ? "supported" : "unsupported");

    return gpu;
}

void raiseCurrentThreadGpuPriority(ID3D12Device& device)
{
    ComPtr<IDXGIDevice> dxgiDevice;

    if (const HRESULT result = device.QueryInterface(IID_PPV_ARGS(&dxgiDevice)); FAILED(result))
    {
        spdlog::warn("{}", describeFailure("ID3D12Device as IDXGIDevice", result));
        return;
    }

    if (const HRESULT result = dxgiDevice->SetGPUThreadPriority(kHighestGpuThreadPriority);
        FAILED(result))
    {
        spdlog::warn("{}", describeFailure("SetGPUThreadPriority", result));
        return;
    }

    spdlog::info("GPU thread priority {} on thread {}", kHighestGpuThreadPriority,
                 GetCurrentThreadId());
}

} // namespace lynx::render
