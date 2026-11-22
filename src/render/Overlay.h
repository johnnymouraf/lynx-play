#pragma once

#include "render/GpuTimer.h"

#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <d3d12.h>
#include <windows.h>
#include <wrl/client.h>

struct ImGuiContext;

namespace lynx::render
{

// What the overlay shows about NVIDIA Reflex.
struct ReflexOverlayStats
{
    enum class State
    {
        // No NVIDIA driver, or NVAPI refused it.
        Unavailable,
        // Configured, waiting for the first latency report.
        Waiting,
        Active,
        // The driver's sleep outran the frame's GPU work; off for the stream.
        Stalled,
        // NvAPI_D3D_Sleep failed; markers and report may still work.
        Failed,
    };

    State state = State::Unavailable;
    // Why when Unavailable, in the interface's language.
    std::string reason;
    std::string mode;
    bool lowLatencyEffective = false;
    bool driverSeesSleep = false;
    double sleepMeanMs = 0.0;
    double sleepMaxMs = 0.0;
    bool latencyValid = false;
    double latencyTotalMs = 0.0;
    double latencyGpuMs = 0.0;
    double latencyRenderMs = 0.0;
    double latencyPresentMs = 0.0;
    double latencyDriverMs = 0.0;
    double latencyQueueMs = 0.0;
};

struct OverlayStats
{
    bool gpuTimingsValid = false;
    std::array<double, kEffectStageCount> stageMilliseconds{};
    double totalMilliseconds = 0.0;
    ReflexOverlayStats reflex;
};

// The minimal in-stream panel: the GPU time of each pipeline stage and
// Reflex's latency report, drawn with Dear ImGui into the stream's back
// buffer. It only shows statistics; the full menu belongs to the overlay's
// own milestone.
class Overlay
{
public:
    [[nodiscard]] static std::expected<std::unique_ptr<Overlay>, std::string> create(
        ID3D12Device& device, ID3D12CommandQueue& queue, HWND window, UINT bufferCount,
        DXGI_FORMAT format, std::span<const std::uint8_t> font);

    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    Overlay(Overlay&&) = delete;
    Overlay& operator=(Overlay&&) = delete;
    ~Overlay();

    // The target must be in the RENDER_TARGET state.
    void render(ID3D12GraphicsCommandList& commandList, D3D12_CPU_DESCRIPTOR_HANDLE target,
                const OverlayStats& stats);

private:
    Overlay() = default;

    void allocateView(D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu);
    void freeView(D3D12_CPU_DESCRIPTOR_HANDLE cpu);

    ID3D12Device* device_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> viewHeap_;
    UINT viewDescriptorSize_ = 0;
    std::vector<bool> viewsUsed_;
    ImGuiContext* context_ = nullptr;
    // The atlas keeps pointing at these bytes; they must outlive the context.
    std::vector<std::uint8_t> fontData_;
};

} // namespace lynx::render
