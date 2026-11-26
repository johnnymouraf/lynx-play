#pragma once

#include <cstdint>
#include <string>

namespace mfplay::core
{

enum class ReflexUnavailable
{
    None,
    NoNvidiaGpu,
    NoDriver,
    DriverFailed,
};
struct ReflexAvailability
{
    bool available = false;
    ReflexUnavailable reason = ReflexUnavailable::NoNvidiaGpu;
    // For the log; English, like every other log line.
    std::string detail;
};
// Why RTX Video Super Resolution can't run here, for the RTX screen.
enum class VsrUnavailable
{
    None,
    NoNvidiaGpu,
    NoD3D11,
    FormatUnsupported,
    ProcessorFailed,
    ExtensionRejected,
};
struct VsrAvailability
{
    bool available = false;
    VsrUnavailable reason = VsrUnavailable::NoNvidiaGpu;
    // For the log; English, like every other log line.
    std::string detail;
};
// Whether RTX Video HDR's extension is accepted on the stream's adapter. The
// NVIDIA App's own toggle and the display's HDR state are separate.
struct RtxHdrAvailability
{
    bool available = false;
    // For the log; English, like every other log line.
    std::string detail;
};
// Why optical flow can't run here, for the RTX screen.
enum class OpticalFlowUnavailable
{
    None,
    // The stream renders on a GPU that is not NVIDIA.
    NoNvidiaGpu,
    // nvofapi64.dll is missing from System32.
    NoDriver,
    // The driver's runtime is older than API 5.0.
    DriverTooOld,
    // The driver refused the D3D12 instance, formats or capabilities.
    DriverFailed,
    // The driver doesn't accept BGRA8 input or the grid output formats.
    FormatUnsupported,
    // The picture is larger than the engine's caps.
    SizeUnsupported,
    // A session that worked stopped responding.
    SessionFailed,
};
struct OpticalFlowAvailability
{
    bool available = false;
    OpticalFlowUnavailable reason = OpticalFlowUnavailable::NoNvidiaGpu;
    // For the log; English, like every other log line.
    std::string detail;
};
// Why DLSS Frame Generation can't run here, for the RTX screen.
enum class FrameGenerationUnavailable
{
    None,
    // The stream renders on a GPU that is not NVIDIA.
    NoNvidiaGpu,
    // The build has no NGX SDK, or nvngx_dlssg.dll isn't next to the app.
    RuntimeMissing,
    // Windows' hardware GPU scheduling (HwSchMode = 2) is off.
    HagsOff,
    // The driver is older than the runtime's minimum.
    DriverTooOld,
    // Below Ada, where the runtime doesn't offer the feature.
    NotAvailable,
    // The driver refused NGX or the runtime failed.
    DriverFailed,
};
struct FrameGenerationAvailability
{
    bool available = false;
    FrameGenerationUnavailable reason = FrameGenerationUnavailable::NoNvidiaGpu;
    // For the log; English, like every other log line.
    std::string detail;
    bool hagsOn = false;
    // 1 on Ada (2x only), 5 on Blackwell (6x).
    std::uint32_t multiFrameCountMax = 0;
};
// Why DLSS Super Resolution can't run on this machine.
enum class DlssUnavailable
{
    None,
    // The stream renders on a GPU that is not NVIDIA.
    NoNvidiaGpu,
    // The flow engine, DLSS's source of motion vectors, is off or failed.
    NoOpticalFlow,
    // DLSS needs Ada or newer; the mission scopes it there.
    GpuTooOld,
    // The GPU architecture couldn't be read from the driver.
    ArchitectureUnknown,
    // The driver's NGX core, nvngx.dll, is missing from System32.
    NoNgxCore,
    // The NGX runtime, nvngx_dlss.dll, is missing from the app's folder.
    NoDlssRuntime,
    // NVSDK_NGX_D3D12_Init refused the device or the project id.
    InitFailed,
    // The feature couldn't be created for the picture; VSR draws instead.
    FeatureFailed,
};
struct DlssAvailability
{
    bool available = false;
    DlssUnavailable reason = DlssUnavailable::NoNvidiaGpu;
    // For the log; English, like every other log line.
    std::string detail;
};
// What the monitor under a window can show, as DisplayConfig reports it.
struct HdrDisplayState
{
    std::string name;
    bool hdrSupported = false;
    // HDR is enabled in Windows and the monitor is in its HDR mode.
    bool hdrActive = false;
    // Windows' SDR reference white; 200 nits when the system doesn't report
    // one that tone mapping can use.
    double sdrWhiteNits = 200.0;
};
enum class PresentTiming
{
    // Waits for the display's refresh.
    VSync,
    // Shows the frame as soon as it is ready, tearing when the display allows.
    Immediate,
};

} // namespace mfplay::core
