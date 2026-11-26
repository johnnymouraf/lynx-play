#pragma once

#include "core/RtxCapabilities.h"
#include "core/Windows.h"

#include <cstdint>
#include <expected>
#include <string>

#include <d3d12.h>
#include <wrl/client.h>

struct NVSDK_NGX_Parameter;

namespace mfplay::render
{

using core::DlssUnavailable;
using core::DlssAvailability;

// The NVAPI architecture id of Ada (AD100); newer chips have larger ids.
inline constexpr std::uint32_t kAdaArchitecture = 0x00000190;

// Whether DLSS Super Resolution supports this GPU architecture.
[[nodiscard]] bool dlssArchitectureSupported(std::uint32_t architecture);

// Whether DLSS Super Resolution can run here: an NVIDIA adapter of Ada or
// newer, the driver's NGX core and the app's DLSS runtime. Never fails the
// app on a machine without them.
[[nodiscard]] DlssAvailability probeDlss();

// An NVSDK_NGX_Result with the runtime's own text, for logs.
[[nodiscard]] std::string ngxResultText(std::uint32_t result);

// The process's NGX instance for the stream's device: NVSDK_NGX initialized
// once with a project id and shut down with the renderer. DLSS Super
// Resolution and DLSS Frame Generation are both created against it, and both
// allocate their parameters through it.
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
    [[nodiscard]] bool initialized() const;
    // What the instance runs, for the log.
    [[nodiscard]] const std::string& description() const;

    // A parameter block for creating and evaluating a feature; the caller
    // destroys it with destroyParameters().
    [[nodiscard]] std::expected<NVSDK_NGX_Parameter*, std::string> allocateParameters() const;
    // The device's capability map; the caller destroys it the same way.
    [[nodiscard]] std::expected<NVSDK_NGX_Parameter*, std::string> capabilityParameters() const;
    static void destroyParameters(NVSDK_NGX_Parameter* parameters);

private:
    NgxHost() = default;

    void shutdown();

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    std::string description_;
    bool initialized_ = false;
};

} // namespace mfplay::render
