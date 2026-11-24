#pragma once

#include "render/AdapterSelection.h"

#include <expected>
#include <string>
#include <vector>

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace mfplay::render
{

using Microsoft::WRL::ComPtr;

// The Direct3D 12 device the stream renders on, with its presentation queue.
struct GpuDevice
{
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter4> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    // One D3D11 device on the same adapter for two things: DXGI's GPU thread
    // priority, which only exists on its IDXGIDevice, and the video
    // processor's interop with Direct3D 12 shared textures and fence.
    ComPtr<ID3D11Device> d3d11Device;
    ComPtr<ID3D11DeviceContext> d3d11Context;
    ComPtr<IDXGIDevice> dxgiDevice;
    AdapterInfo info;
    bool supportsTearing = false;
};

// The adapter the stream renders on, for subsystems that need a device of
// their own on it (the video processor's D3D11 device).
struct StreamAdapter
{
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter4> adapter;
    AdapterInfo info;
};

// The hardware adapters in DXGI's high-performance order.
[[nodiscard]] std::expected<std::vector<AdapterInfo>, std::string> listAdapters();

[[nodiscard]] std::expected<StreamAdapter, std::string> findStreamAdapter();

[[nodiscard]] std::expected<GpuDevice, std::string> createGpuDevice();

// Raises the calling thread's priority to DXGI's highest for GPU work. Call it
// on the render and decode threads, once each.
void raiseCurrentThreadGpuPriority(IDXGIDevice& device);

} // namespace mfplay::render
