#pragma once

#include "render/Layout.h"
#include "render/NgxHost.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <string>

#include <d3d12.h>

namespace lynx::render
{

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

// DLSS Frame Generation through NGX: the feature and the two pictures it
// writes, at the swap chain's size and format. The real picture is the
// caller's, composed offscreen; the generated one is written here.
class FrameGeneration
{
public:
    // Checks the GPU, HAGS and the runtime without a stream; never fails the
    // app on a machine without an NVIDIA GPU.
    [[nodiscard]] static FrameGenerationAvailability probe();

    // Creates the feature and its resources on `commandList`, which the
    // caller executes. The host must be initialized for the device. The
    // first creation can take seconds while the runtime compiles its model.
    [[nodiscard]] static std::expected<FrameGeneration, std::string> create(
        NgxHost& host, ID3D12Device& device, ID3D12GraphicsCommandList& commandList,
        SurfaceSize extent, DXGI_FORMAT backbufferFormat);

    FrameGeneration(const FrameGeneration&) = delete;
    FrameGeneration& operator=(const FrameGeneration&) = delete;
    FrameGeneration(FrameGeneration&&) noexcept;
    FrameGeneration& operator=(FrameGeneration&&) noexcept;
    ~FrameGeneration();

    [[nodiscard]] bool configuredFor(SurfaceSize extent, DXGI_FORMAT backbufferFormat) const;

    // The constant depth the runtime expects: R32_FLOAT filled with 0.9, so
    // its 1 / (1 - depth) linearization stays finite and far.
    [[nodiscard]] ID3D12Resource* depth() const;
    // The interpolated picture, at the back buffer's size and format. It is
    // left in COPY_SOURCE after evaluate; call generatedCopied() once the
    // caller's list returns it to COMMON.
    [[nodiscard]] ID3D12Resource* generated() const;
    [[nodiscard]] SurfaceSize extent() const;
    [[nodiscard]] DXGI_FORMAT format() const;

    // One generated picture between the previous real picture and
    // `backbuffer` (both in NON_PIXEL_SHADER_RESOURCE). The motion vectors
    // are current-to-previous pixels at the extent, in
    // NON_PIXEL_SHADER_RESOURCE.
    [[nodiscard]] std::expected<void, std::string> evaluate(
        ID3D12GraphicsCommandList& commandList, ID3D12Resource& backbuffer,
        ID3D12Resource& motionVectors, bool reset, std::uint64_t frameId);

    void generatedCopied();

    [[nodiscard]] const std::string& description() const;
    [[nodiscard]] std::uint64_t evaluations() const;
    [[nodiscard]] bool failed() const;

    // Releases the feature; the GPU must be idle.
    void release();

private:
    struct Impl;

    explicit FrameGeneration(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

} // namespace lynx::render
