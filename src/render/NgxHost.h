#pragma once

#include "core/Windows.h"

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include <d3d12.h>
#include <wrl/client.h>

namespace lynx::render
{

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

// Whether DLSS Super Resolution can run here: an NVIDIA adapter of Ada or
// newer, the driver's NGX core and the app's DLSS runtime. Never fails the
// app on a machine without them.
[[nodiscard]] DlssAvailability probeDlss();

// An NVSDK_NGX_Result with the runtime's own text, for logs.
[[nodiscard]] std::string ngxResultText(std::uint32_t result);

// The process's NGX instance for the stream's device: NVSDK_NGX initialized
// once with a project id and shut down with the renderer. The features
// (DLSS Super Resolution, and DLSS Frame Generation later) are created
// against it.
class NgxHost
{
public:
    [[nodiscard]] static std::expected<NgxHost, std::string> create(ID3D12Device& device);

    NgxHost(const NgxHost&) = delete;
    NgxHost& operator=(const NgxHost&) = delete;
    NgxHost(NgxHost&& other) noexcept;
    NgxHost& operator=(NgxHost&& other) noexcept;
    ~NgxHost();

    [[nodiscard]] ID3D12Device* device() const;
    // What the instance runs, for the log.
    [[nodiscard]] const std::string& description() const;

private:
    NgxHost() = default;

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    std::string description_;
    bool initialized_ = false;
};

} // namespace lynx::render
