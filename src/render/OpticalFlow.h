#pragma once

#include "render/Layout.h"

#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <d3d12.h>

namespace mfplay::render
{

// The grid NVIDIA's optical flow engine uses: one vector per 4x4 picture
// pixels. Grid 1 and 2 exist on Ampere and newer; 4 is the cheapest and the
// one every supported GPU takes.
inline constexpr std::uint32_t kFlowGridSize = 4;
// NvOF stores flow vectors as S10.5 fixed point: five fractional bits.
inline constexpr float kFlowFixedPointScale = 32.0F;

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

// One motion vector in pixels, current picture to previous picture.
struct FlowVector
{
    float x = 0.0F;
    float y = 0.0F;
};

// S10.5 fixed point to pixels.
[[nodiscard]] FlowVector flowVectorPixels(std::int16_t flowX, std::int16_t flowY);

// The vector grid the engine writes for a picture: ceil(size / grid).
[[nodiscard]] SurfaceSize flowGridExtent(SurfaceSize picture,
                                          std::uint32_t grid = kFlowGridSize);

// The nearest-neighbour densify step: the vector of the cell covering a
// pixel, or nullopt outside the picture. `vectors` holds interleaved x and y
// in S10.5, as many as `gridExtent` has cells.
[[nodiscard]] std::optional<FlowVector> flowVectorAt(std::span<const std::int16_t> vectors,
                                                      SurfaceSize gridExtent, SurfaceSize picture,
                                                      std::uint32_t x, std::uint32_t y,
                                                      std::uint32_t grid = kFlowGridSize);

// The order a session is released in: the engine's last flow passes, its
// registered pictures go, the pictures are released, then the session. A
// driver fault in one step must not skip the ones after it.
enum class SessionReleaseStep
{
    WaitForLastFlow,
    UnregisterCostResources,
    UnregisterFlowResources,
    UnregisterInputResources,
    ReleaseCostPictures,
    ReleaseFlowPictures,
    ReleaseInputPictures,
    DestroySession,
};

inline constexpr std::array<SessionReleaseStep, 8> kSessionReleaseSteps{
    SessionReleaseStep::WaitForLastFlow,
    SessionReleaseStep::UnregisterCostResources,
    SessionReleaseStep::UnregisterFlowResources,
    SessionReleaseStep::UnregisterInputResources,
    SessionReleaseStep::ReleaseCostPictures,
    SessionReleaseStep::ReleaseFlowPictures,
    SessionReleaseStep::ReleaseInputPictures,
    SessionReleaseStep::DestroySession,
};

[[nodiscard]] std::string_view sessionReleaseStepName(SessionReleaseStep step);

// Runs each release step in order; a fault in one is reported to `faulted` and
// the steps after it still run.
void runSessionRelease(const std::function<void(SessionReleaseStep)>& step,
                       const std::function<void(SessionReleaseStep, std::uint32_t)>& faulted);

// NVIDIA's optical flow engine through the driver's nvofapi64.dll, fed by the
// presenter's BGRA8 intermediates on the renderer's device. Each execute runs
// on the engine's own queue and is ordered by fence points: the renderer's
// queue signals the input value, and the engine signals the output value when
// the vectors and the cost are ready. One instance per stream window.
class OpticalFlow
{
public:
    // Checks the driver and the stream adapter without a stream; never fails
    // the app on a machine without an NVIDIA GPU.
    [[nodiscard]] static OpticalFlowAvailability probe();

    [[nodiscard]] static std::expected<OpticalFlow, std::string> create(ID3D12Device& device,
                                                                        UINT slotCount);

    OpticalFlow(const OpticalFlow&) = delete;
    OpticalFlow& operator=(const OpticalFlow&) = delete;
    OpticalFlow(OpticalFlow&&) noexcept;
    OpticalFlow& operator=(OpticalFlow&&) noexcept;
    ~OpticalFlow();

    // Whether the session is built for this size with these pictures.
    [[nodiscard]] bool configuredFor(SurfaceSize picture) const;

    // (Re)builds the engine for the picture size, registering one input
    // picture per slot. The GPU must be idle, as the old session is released.
    [[nodiscard]] std::expected<void, std::string> configure(
        SurfaceSize picture, std::span<ID3D12Resource* const> inputs);

    // Reserves the fence value the renderer's queue must signal once the
    // current picture is complete. execute() signals one past it.
    [[nodiscard]] std::uint64_t nextInputValue();

    // Starts the flow for slot's picture against the previous slot's. The
    // returned value is the fence value to wait for.
    [[nodiscard]] std::expected<std::uint64_t, std::string> execute(UINT slot,
                                                                    std::uint64_t inputValue);

    // The fence the renderer signals and waits on.
    [[nodiscard]] ID3D12Fence* fence() const;

    // The last output value execute() returned; zero before the first run.
    // Waiting on it covers every flow the engine has written.
    [[nodiscard]] std::uint64_t lastOutputValue() const;

    // The R16G16_SINT vectors and the R8_UINT cost of a slot, kept in
    // D3D12_RESOURCE_STATE_COMMON while the engine owns them.
    [[nodiscard]] ID3D12Resource* flow(UINT slot) const;
    [[nodiscard]] ID3D12Resource* cost(UINT slot) const;
    [[nodiscard]] SurfaceSize gridExtent() const;

    // What the session runs, for the log.
    [[nodiscard]] const std::string& description() const;

private:
    struct Impl;

    explicit OpticalFlow(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

} // namespace mfplay::render
