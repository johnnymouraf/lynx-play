#pragma once

#include <expected>
#include <string>

#include <d3d12.h>

#if defined(LYNX_HAVE_NGX)
#include <nvsdk_ngx.h>
#else
struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_Handle;
#endif

namespace lynx::render
{

// The NGX core for one Direct3D 12 device: initialization, its parameter
// blocks and the runtime search path. NGX is process-wide and not thread
// safe, so one host per stream window, used on the renderer's thread.
class NgxHost
{
public:
    NgxHost() = default;
    ~NgxHost();

    NgxHost(const NgxHost&) = delete;
    NgxHost& operator=(const NgxHost&) = delete;
    NgxHost(NgxHost&&) = delete;
    NgxHost& operator=(NgxHost&&) = delete;

    // Initializes NGX for this device, pointing its feature search at the
    // executable's folder; the app data folder is %LOCALAPPDATA%\LynxPlay\NGX.
    [[nodiscard]] std::expected<void, std::string> initialize(ID3D12Device& device);

    // Releases NGX; the device must be idle and every feature released.
    void shutdown();

    [[nodiscard]] bool initialized() const;
    [[nodiscard]] const std::string& description() const;

    // A parameter block for creating and evaluating a feature; the caller
    // destroys it with destroyParameters().
    [[nodiscard]] std::expected<NVSDK_NGX_Parameter*, std::string> allocateParameters() const;
    // The device's capability map; the caller destroys it the same way.
    [[nodiscard]] std::expected<NVSDK_NGX_Parameter*, std::string> capabilityParameters() const;
    static void destroyParameters(NVSDK_NGX_Parameter* parameters);

private:
    ID3D12Device* device_ = nullptr;
    std::string description_;
};

} // namespace lynx::render
