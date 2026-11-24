#pragma once

#include "core/ReflexMode.h"
#include "core/Settings.h"
#include "core/StreamMetrics.h"
#include "render/GpuTimer.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <d3d12.h>
#include <windows.h>
#include <wrl/client.h>

struct ImGuiContext;
struct ImFont;

namespace mfplay::render
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

// How a pipeline stage that can be unavailable reads in the statistics panel.
enum class StageOverlayState
{
    Off,
    Running,
    Unavailable,
};

struct OverlayStats
{
    // The video path's counters over the last two seconds.
    core::StreamMetricsSnapshot video;
    std::uint32_t outputWidth = 0;
    std::uint32_t outputHeight = 0;
    // The stream carries PQ, the surface is HDR10 and RTX Video HDR ran.
    bool hdrStream = false;
    bool hdrSwapChain = false;
    bool rtxVideoHdr = false;

    bool gpuTimingsValid = false;
    std::array<double, kEffectStageCount> stageMilliseconds{};
    double totalMilliseconds = 0.0;
    StageOverlayState opticalFlow = StageOverlayState::Off;
    StageOverlayState dlss = StageOverlayState::Off;
    // The scaling path and DLSS Frame Generation the renderer is applying,
    // for the quick menu's toggles.
    core::Upscaling upscaling = core::Upscaling::Shader;
    core::FrameGenerationMode frameGeneration = core::FrameGenerationMode::Off;

    // The published cadence, once two windows agree.
    bool cadenceValid = false;
    double cadenceFramesPerSecond = 0.0;
    std::uint32_t cadenceRepeatedFrames = 0;
    std::uint32_t cadenceWindowFrames = 0;

    ReflexOverlayStats reflex;
};

// The font files the overlay draws with; a missing one falls back to Dear
// ImGui's built-in font.
struct OverlayFonts
{
    std::shared_ptr<const std::vector<std::uint8_t>> body;
    std::shared_ptr<const std::vector<std::uint8_t>> label;
    std::shared_ptr<const std::vector<std::uint8_t>> title;
    std::shared_ptr<const std::vector<std::uint8_t>> mono;
    std::shared_ptr<const std::vector<std::uint8_t>> monoStrong;
};

// What the overlay starts with, before the player changes anything.
struct OverlayOptions
{
    bool statisticsVisible = false;
    core::ReflexMode reflex = core::ReflexMode::On;
    // Presents with no sync interval, letting the display tear.
    bool lowestLatency = false;
    bool microphoneMuted = false;
    // The controller the microphone toggle names, if any.
    std::string microphoneDetail;
    // The console (or file) being played; the quick menu's session line.
    std::string sessionName;
    // The stream window's mode and remembered windowed size, for the quick
    // menu's window row.
    core::WindowMode windowMode = core::WindowMode::Fullscreen;
    core::WindowSize windowSize = core::kDefaultWindowSize;
    // The back buffer is HDR10: Dear ImGui's sRGB colors are converted to PQ
    // at this SDR white level.
    bool hdrOutput = false;
    float sdrWhiteNits = 200.0F;
};

// What the overlay asks the app to do; each runs on the renderer's thread, so
// the app must defer anything that touches its own threads.
struct OverlayActions
{
    std::function<void(core::ReflexMode)> reflexChanged;
    std::function<void(core::Upscaling)> upscalingChanged;
    std::function<void(core::FrameGenerationMode)> frameGenerationChanged;
    std::function<void(bool)> lowestLatencyChanged;
    std::function<void(bool)> microphoneMutedChanged;
    std::function<void(bool)> statisticsVisibleChanged;
    std::function<void(bool)> menuOpenChanged;
    // The quick menu's window row: the layout the player cycled to.
    std::function<void(core::WindowMode, core::WindowSize)> windowLayoutChanged;
    std::function<void()> disconnect;
};

// The controller state the overlay navigates with while its menu is open, and
// the debug view's snapshot of what the console receives.
struct OverlayGamepad
{
    struct Touch
    {
        bool down = false;
        std::uint16_t x = 0;
        std::uint16_t y = 0;
    };

    bool connected = false;
    bool cross = false;
    bool circle = false;
    bool square = false;
    bool triangle = false;
    bool dpadUp = false;
    bool dpadDown = false;
    bool dpadLeft = false;
    bool dpadRight = false;
    bool l1 = false;
    bool r1 = false;
    bool l3 = false;
    bool r3 = false;
    bool options = false;
    bool create = false;
    bool touchpad = false;
    bool ps = false;
    float leftX = 0.0F;
    float leftY = 0.0F;
    float rightX = 0.0F;
    float rightY = 0.0F;

    // The debug view: the buttons by chiaki's names, both trigger levels,
    // both touchpad fingers in the console's coordinates, and the sensors.
    std::string buttons;
    std::uint8_t leftTrigger = 0;
    std::uint8_t rightTrigger = 0;
    std::array<Touch, 2> touches{};
    std::array<float, 3> gyro{};
    std::array<float, 3> accel{};
};

// The in-stream overlay, drawn with Dear ImGui into the stream's back buffer:
// the statistics panel on the left and the quick menu on the right, both
// hidden until F1 (menu) or F2 (statistics) asks for them.
class Overlay
{
public:
    [[nodiscard]] static std::expected<std::unique_ptr<Overlay>, std::string> create(
        ID3D12Device& device, ID3D12CommandQueue& queue, HWND window, UINT bufferCount,
        DXGI_FORMAT format, OverlayFonts fonts, OverlayOptions options, OverlayActions actions);

    Overlay(const Overlay&) = delete;
    Overlay& operator=(const Overlay&) = delete;
    Overlay(Overlay&&) = delete;
    Overlay& operator=(Overlay&&) = delete;
    ~Overlay();

    // The target must be in the RENDER_TARGET state.
    void render(ID3D12GraphicsCommandList& commandList, D3D12_CPU_DESCRIPTOR_HANDLE target,
                const OverlayStats& stats, const OverlayGamepad& gamepad);

    // Rebuilds the Direct3D 12 backend for a new back buffer format; safe to
    // call while handleMessage() runs on the window's thread.
    void setOutputFormat(DXGI_FORMAT format, bool hdrOutput, float sdrWhiteNits);

    // Safe from the interface's thread; applied by the next frame.
    void toggleMenu();
    void toggleStatistics();
    // Safe from the window's thread; the quick menu's window row shows this
    // before the next frame.
    void setWindowLayout(core::WindowMode mode, core::WindowSize size);
    [[nodiscard]] bool menuOpen() const;
    [[nodiscard]] bool statisticsVisible() const;
    // Whether anything is on screen, so the renderer keeps drawing.
    [[nodiscard]] bool visible() const;

    // Feeds Dear ImGui's Win32 backend; safe to call beside render().
    bool handleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

private:
    Overlay() = default;

    void allocateView(D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu);
    void freeView(D3D12_CPU_DESCRIPTOR_HANDLE cpu);
    void loadFonts(const OverlayFonts& fonts);
    [[nodiscard]] bool createDx12Backend();
    void sendGamepad(const OverlayGamepad& gamepad);
    void reportState();

    ID3D12Device* device_ = nullptr;
    ID3D12CommandQueue* queue_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> viewHeap_;
    UINT viewDescriptorSize_ = 0;
    std::vector<bool> viewsUsed_;
    ImGuiContext* context_ = nullptr;
    // The atlas keeps pointing at these bytes; they must outlive the context.
    OverlayFonts fontData_;
    ImFont* bodyFont_ = nullptr;
    ImFont* labelFont_ = nullptr;
    ImFont* titleFont_ = nullptr;
    ImFont* monoFont_ = nullptr;
    ImFont* monoStrongFont_ = nullptr;

    OverlayOptions options_;
    OverlayActions actions_;
    UINT bufferCount_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
    bool hdrOutput_ = false;
    float sdrWhiteNits_ = 200.0F;
    std::atomic<bool> menuOpen_{false};
    std::atomic<bool> statisticsVisible_{false};
    std::atomic<core::WindowMode> windowMode_{core::WindowMode::Fullscreen};
    std::atomic<core::WindowSize> windowSize_{core::kDefaultWindowSize};
    bool reportedMenuOpen_ = false;
    bool reportedStatisticsVisible_ = false;
    core::ReflexMode reflex_ = core::ReflexMode::Off;
    bool lowestLatency_ = false;
    bool microphoneMuted_ = false;
    // Paces the controller debug line written while the statistics are open.
    std::chrono::steady_clock::time_point nextControllerLog_{};
};

} // namespace mfplay::render
