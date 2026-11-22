#pragma once

#include "render/AdapterSelection.h"

#include <expected>
#include <string>
#include <vector>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace lynx::render
{

using Microsoft::WRL::ComPtr;

// The Direct3D 12 device the stream renders on, with its presentation queue.
struct GpuDevice
{
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter4> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    AdapterInfo info;
    bool supportsTearing = false;
};

// The hardware adapters in DXGI's high-performance order.
[[nodiscard]] std::expected<std::vector<AdapterInfo>, std::string> listAdapters();

[[nodiscard]] std::expected<GpuDevice, std::string> createGpuDevice();

} // namespace lynx::render
