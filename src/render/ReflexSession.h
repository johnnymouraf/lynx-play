#pragma once

#include "core/ReflexMode.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <d3d12.h>

namespace mfplay::render
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

// The markers NvAPI tracks for the latency report, in the order a frame sends
// them.
enum class LatencyMarker
{
    SimulationStart,
    SimulationEnd,
    RenderSubmitStart,
    RenderSubmitEnd,
    PresentStart,
    PresentEnd,
    // On the out-of-band queue: the work that copies a generated picture and
    // the present of that picture.
    OutOfBandRenderSubmitStart,
    OutOfBandRenderSubmitEnd,
    OutOfBandPresentStart,
    OutOfBandPresentEnd,
};

// NvAPI_D3D_GetLatency's latest report, in milliseconds.
struct ReflexLatency
{
    bool valid = false;
    std::uint64_t frameId = 0;
    double totalMilliseconds = 0.0;
    double simulationMilliseconds = 0.0;
    double renderSubmitMilliseconds = 0.0;
    double presentMilliseconds = 0.0;
    double driverMilliseconds = 0.0;
    double queueMilliseconds = 0.0;
    double gpuMilliseconds = 0.0;
};

// NvAPI_D3D_GetSleepStatus: what the driver says it is doing.
struct ReflexSleepStatus
{
    bool valid = false;
    bool lowLatency = false;
    bool driverSeesSleep = false;
    bool fullscreenFlip = false;
    std::uint32_t sleepIntervalUs = 0;
};

// Notices when the driver's sleep runs longer than the frame's GPU work often
// enough to be pacing the stream: the sleep can double-pace a 60 Hz VSync
// chain and halve the presented rate.
class SleepStallDetector
{
public:
    // Sleeps shorter than this are never a stall, even when the GPU work is
    // tiny.
    static constexpr std::uint32_t kFloorUs = 2000;
    static constexpr std::size_t kWindow = 16;
    static constexpr std::size_t kThreshold = 10;

    // `gpuWorkUs` is the GPU time of the last completed frame. True once
    // kThreshold of the last kWindow sleeps exceeded it.
    [[nodiscard]] bool observe(std::uint32_t sleepUs, std::uint32_t gpuWorkUs);

private:
    std::array<bool, kWindow> exceeded_{};
    std::size_t exceededCount_ = 0;
    std::size_t next_ = 0;
};

[[nodiscard]] std::string_view reflexModeName(core::ReflexMode mode);

// NVIDIA Reflex through the driver's nvapi64.dll: sleep mode, the per-frame
// sleep and latency markers, and the driver's latency report. Everything
// here runs on the renderer's thread.
class ReflexSession
{
public:
    ReflexSession() = default;
    ~ReflexSession() = default;

    ReflexSession(const ReflexSession&) = delete;
    ReflexSession& operator=(const ReflexSession&) = delete;
    ReflexSession(ReflexSession&&) = delete;
    ReflexSession& operator=(ReflexSession&&) = delete;

    // Loads nvapi64.dll and checks the stream would run on an NVIDIA adapter;
    // never fails the app on a machine without one.
    [[nodiscard]] static ReflexAvailability probe();

    // Queries the Reflex entry points and initializes NVAPI for this process.
    [[nodiscard]] bool open();

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool failed() const;
    [[nodiscard]] bool stalled() const;
    [[nodiscard]] core::ReflexMode mode() const;
    [[nodiscard]] const std::string& detail() const;

    // Sends the sleep mode to the driver; only mode, device or swap-chain
    // changes need it.
    void configure(core::ReflexMode mode, ID3D12Device* device);

    // One call per accepted source frame, before its work: NvAPI_D3D_Sleep,
    // measured. Returns the frame's marker id, strictly increasing.
    [[nodiscard]] std::uint64_t beginSourceFrame(std::uint32_t gpuWorkUs);

    void mark(std::uint64_t frameId, LatencyMarker marker);

    // Tells the driver this queue presents pictures the application did not
    // render. Frame generation is gated on it, so a failure disables the
    // feature for the stream.
    [[nodiscard]] bool enableOutOfBand(ID3D12CommandQueue& queue);
    [[nodiscard]] bool outOfBandEnabled() const;

    // The out-of-band markers for the copies and presents of generated
    // pictures; they target the queue, not the device.
    void markOutOfBand(std::uint64_t frameId, LatencyMarker marker);

    // Polls the sleep status and the latency report at most once a second.
    void poll();

    [[nodiscard]] const ReflexLatency& latency() const;
    [[nodiscard]] const ReflexSleepStatus& sleepStatus() const;
    [[nodiscard]] std::uint64_t sleepCount() const;
    [[nodiscard]] std::uint64_t markerSets() const;
    [[nodiscard]] double lastSleepMilliseconds() const;
    // Over the last 64 sleeps.
    [[nodiscard]] double meanSleepMilliseconds() const;
    [[nodiscard]] double maxSleepMilliseconds() const;

    // One log line with the counts, the sleep statistics and the report.
    void logSummary() const;

    // Turns low latency off while the device is still alive.
    void shutdown();

private:
    [[nodiscard]] bool applySleepMode(core::ReflexMode mode);
    void addSleepSample(double milliseconds);
    void disableAfterStall(std::uint32_t sleepUs, std::uint32_t gpuWorkUs);

    bool available_ = false;
    bool failed_ = false;
    bool stalled_ = false;
    bool configured_ = false;
    bool sleepStatusFailureLogged_ = false;
    bool latencyFailureLogged_ = false;
    bool markerFailureLogged_ = false;
    bool markersStarted_ = false;
    bool outOfBandMarkersLogged_ = false;
    std::string detail_;
    ID3D12Device* device_ = nullptr;
    ID3D12CommandQueue* outOfBandQueue_ = nullptr;
    core::ReflexMode mode_ = core::ReflexMode::Off;
    core::ReflexMode appliedMode_ = core::ReflexMode::Off;
    std::uint64_t nextFrameId_ = 1;
    std::uint64_t sleepCount_ = 0;
    std::uint64_t markerSets_ = 0;
    std::uint64_t outOfBandMarkers_ = 0;
    std::uint64_t framesMarked_ = 0;
    double lastSleepMilliseconds_ = 0.0;
    std::array<double, 64> sleepWindow_{};
    std::size_t sleepWindowCount_ = 0;
    std::size_t sleepWindowNext_ = 0;
    SleepStallDetector stallDetector_;
    ReflexLatency latency_;
    ReflexSleepStatus sleepStatus_;
    std::chrono::steady_clock::time_point nextPoll_;
};

} // namespace mfplay::render
