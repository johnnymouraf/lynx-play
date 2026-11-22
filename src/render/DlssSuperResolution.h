#pragma once

#include "core/Windows.h"
#include "render/Layout.h"
#include "render/NgxHost.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <string>

#include <d3d12.h>

namespace lynx::render
{

// The picture DLSS runs on: the render size of the 8-bit SDR frame it reads
// and the on-screen size it writes.
struct DlssPicture
{
    SurfaceSize input;
    SurfaceSize output;
    // The input is the presenter's tone-mapped BGRA8 copy of a PQ stream,
    // rather than the stream's own 10-bit intermediate.
    bool toneMappedInput = false;

    bool operator==(const DlssPicture&) const = default;
};

// DLSS Super Resolution only upscales: the output must be larger than the
// input on both axes and at most three times either one (Ultra Performance).
// A 1:1 window is left to VSR, which also cleans up compression artifacts.
[[nodiscard]] bool dlssSuperResolutionEligible(SurfaceSize input, SurfaceSize output);

// NVIDIA DLSS Super Resolution through NGX, fed by the optical flow's motion
// vectors: the current 8-bit picture, a constant depth and per-pixel motion
// vectors densified from the flow grid, with no jitter and no sharpening.
// One instance per stream window, rebuilt when the sizes change.
class DlssSuperResolution
{
public:
    [[nodiscard]] static std::expected<DlssSuperResolution, std::string> create(
        NgxHost& host, ID3D12CommandQueue& queue, UINT slotCount);

    DlssSuperResolution(const DlssSuperResolution&) = delete;
    DlssSuperResolution& operator=(const DlssSuperResolution&) = delete;
    DlssSuperResolution(DlssSuperResolution&&) noexcept;
    DlssSuperResolution& operator=(DlssSuperResolution&&) noexcept;
    ~DlssSuperResolution();

    [[nodiscard]] bool configuredFor(const DlssPicture& picture) const;

    // Creates the feature and its per-slot pictures for these sizes. The GPU
    // must be idle, as the previous feature and pictures are released; the
    // first creation compiles the model and can take seconds.
    [[nodiscard]] std::expected<void, std::string> configure(const DlssPicture& picture);

    [[nodiscard]] ID3D12Resource* output(UINT slot) const;

    // Densifies `flow`, the engine's R16G16_SINT grid for this slot, into the
    // per-pixel R16G16_FLOAT motion vectors and evaluates the feature into
    // this slot's output. The renderer's queue must have waited for the flow
    // engine's fence before the list executes. `reset` restarts the history
    // after a cut or a source change.
    [[nodiscard]] std::expected<void, std::string> record(
        ID3D12GraphicsCommandList& commandList, UINT slot, ID3D12Resource& color,
        ID3D12Resource& flow, bool reset);

    // What the configured feature runs, for the log.
    [[nodiscard]] const std::string& description() const;

private:
    struct Impl;

    explicit DlssSuperResolution(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

} // namespace lynx::render
