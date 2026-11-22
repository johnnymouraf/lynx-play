#include "render/ReflexSession.h"

#include "render/AdapterSelection.h"
#include "render/GpuDevice.h"

#include <algorithm>
#include <chrono>
#include <format>

#include <spdlog/spdlog.h>

#include <windows.h>

#include <d3d12.h>
#include <nvapi.h>

namespace lynx::render
{

namespace
{

constexpr NvU32 kInitializeId = 0x0150E828;
constexpr NvU32 kGetErrorMessageId = 0x6C2D048C;
constexpr NvU32 kSetSleepModeId = 0xAC1CA9E0;
constexpr NvU32 kSleepId = 0x852CD1D2;
constexpr NvU32 kGetSleepStatusId = 0xAEF96CA1;
constexpr NvU32 kSetLatencyMarkerId = 0xD9984C05;
constexpr NvU32 kGetLatencyId = 0x1A587F9C;

// A few seconds of markers before the driver's report fills up.
constexpr std::uint64_t kLatencyWarmupFrames = 90;
constexpr std::chrono::seconds kPollPeriod{1};

using QueryInterfaceFn = void*(__cdecl*)(NvU32);

// The driver's exported entry points, queried once for the process and never
// unloaded, so a device teardown can't call back into missing code.
struct Nvapi
{
    HMODULE module = nullptr;
    QueryInterfaceFn query = nullptr;
    decltype(&NvAPI_Initialize) initialize = nullptr;
    decltype(&NvAPI_GetErrorMessage) errorMessage = nullptr;
    decltype(&NvAPI_D3D_SetSleepMode) setSleepMode = nullptr;
    decltype(&NvAPI_D3D_Sleep) sleep = nullptr;
    decltype(&NvAPI_D3D_GetSleepStatus) getSleepStatus = nullptr;
    decltype(&NvAPI_D3D_SetLatencyMarker) setLatencyMarker = nullptr;
    decltype(&NvAPI_D3D_GetLatency) getLatency = nullptr;
    NvAPI_Status status = NVAPI_OK;
    std::string detail;
};

std::string statusText(const Nvapi& api, NvAPI_Status status)
{
    NvAPI_ShortString text{};

    if (api.errorMessage != nullptr)
    {
        api.errorMessage(status, text);
    }

    return std::format("{} ({})", static_cast<int>(status), text[0] != '\0' ? text : "unknown");
}

Nvapi loadNvapi()
{
    Nvapi api;

    api.module = LoadLibraryExW(L"nvapi64.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (api.module == nullptr)
    {
        api.detail = "nvapi64.dll isn't installed";
        return api;
    }

    HMODULE pinned = nullptr;

    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(api.module), &pinned) == FALSE)
    {
        api.detail = "nvapi64.dll can't be pinned";
        return api;
    }

    api.query =
        reinterpret_cast<QueryInterfaceFn>(GetProcAddress(api.module, "nvapi_QueryInterface"));

    if (api.query == nullptr)
    {
        api.detail = "nvapi64.dll has no nvapi_QueryInterface";
        return api;
    }

    api.initialize =
        reinterpret_cast<decltype(&NvAPI_Initialize)>(api.query(kInitializeId));
    api.errorMessage =
        reinterpret_cast<decltype(&NvAPI_GetErrorMessage)>(api.query(kGetErrorMessageId));
    api.setSleepMode =
        reinterpret_cast<decltype(&NvAPI_D3D_SetSleepMode)>(api.query(kSetSleepModeId));
    api.sleep = reinterpret_cast<decltype(&NvAPI_D3D_Sleep)>(api.query(kSleepId));
    api.getSleepStatus =
        reinterpret_cast<decltype(&NvAPI_D3D_GetSleepStatus)>(api.query(kGetSleepStatusId));
    api.setLatencyMarker =
        reinterpret_cast<decltype(&NvAPI_D3D_SetLatencyMarker)>(api.query(kSetLatencyMarkerId));
    api.getLatency =
        reinterpret_cast<decltype(&NvAPI_D3D_GetLatency)>(api.query(kGetLatencyId));

    if (api.initialize == nullptr)
    {
        api.detail = "nvapi64.dll has no NvAPI_Initialize";
        return api;
    }

    api.status = api.initialize();

    if (api.status != NVAPI_OK)
    {
        api.detail = std::format("NvAPI_Initialize: {}", statusText(api, api.status));
        return api;
    }

    api.detail = std::format("NVAPI SDK {}", NVAPI_SDK_VERSION);
    return api;
}

const Nvapi& nvapi()
{
    static const Nvapi api = loadNvapi();
    return api;
}

NV_LATENCY_MARKER_TYPE markerType(LatencyMarker marker)
{
    switch (marker)
    {
    case LatencyMarker::SimulationStart:
        return SIMULATION_START;
    case LatencyMarker::SimulationEnd:
        return SIMULATION_END;
    case LatencyMarker::RenderSubmitStart:
        return RENDERSUBMIT_START;
    case LatencyMarker::RenderSubmitEnd:
        return RENDERSUBMIT_END;
    case LatencyMarker::PresentStart:
        return PRESENT_START;
    case LatencyMarker::PresentEnd:
        return PRESENT_END;
    }

    return SIMULATION_START;
}

double microsecondsBetween(NvU64 end, NvU64 begin)
{
    return end >= begin ? static_cast<double>(end - begin) / 1000.0 : 0.0;
}

} // namespace

std::string_view reflexModeName(core::ReflexMode mode)
{
    switch (mode)
    {
    case core::ReflexMode::Off:
        return "off";
    case core::ReflexMode::On:
        return "on";
    case core::ReflexMode::Boost:
        return "boost";
    }

    return "off";
}

bool SleepStallDetector::observe(std::uint32_t sleepUs, std::uint32_t gpuWorkUs)
{
    const std::uint32_t threshold = std::max(gpuWorkUs, kFloorUs);
    const bool exceeded = sleepUs > threshold;

    if (exceeded_.at(next_))
    {
        --exceededCount_;
    }

    exceeded_.at(next_) = exceeded;

    if (exceeded)
    {
        ++exceededCount_;
    }

    next_ = (next_ + 1) % kWindow;
    return exceededCount_ >= kThreshold;
}

ReflexAvailability ReflexSession::probe()
{
    const auto adapters = listAdapters();

    if (!adapters)
    {
        return {
            .available = false,
            .reason = ReflexUnavailable::NoNvidiaGpu,
            .detail = adapters.error(),
        };
    }

    const auto chosen = selectAdapter(*adapters);

    if (!chosen)
    {
        return {
            .available = false,
            .reason = ReflexUnavailable::NoNvidiaGpu,
            .detail = "no hardware adapter can render the stream",
        };
    }

    const AdapterInfo& adapter = adapters->at(*chosen);

    if (adapter.vendorId != kNvidiaVendorId)
    {
        return {
            .available = false,
            .reason = ReflexUnavailable::NoNvidiaGpu,
            .detail = std::format("the stream runs on {} (vendor 0x{:04X})", adapter.name,
                                  adapter.vendorId),
        };
    }

    const Nvapi& api = nvapi();

    if (api.module == nullptr)
    {
        return {
            .available = false,
            .reason = ReflexUnavailable::NoDriver,
            .detail = api.detail,
        };
    }

    if (api.status != NVAPI_OK)
    {
        return {
            .available = false,
            .reason = ReflexUnavailable::DriverFailed,
            .detail = api.detail,
        };
    }

    return {
        .available = true,
        .reason = ReflexUnavailable::None,
        .detail = api.detail,
    };
}

bool ReflexSession::open()
{
    const Nvapi& api = nvapi();
    const bool entryPointsPresent = api.setSleepMode != nullptr && api.sleep != nullptr &&
                                    api.getSleepStatus != nullptr &&
                                    api.setLatencyMarker != nullptr && api.getLatency != nullptr;

    if (api.module == nullptr || api.status != NVAPI_OK)
    {
        detail_ = api.detail;
        return false;
    }

    if (!entryPointsPresent)
    {
        detail_ = "the driver has no Reflex entry points (older than R455)";
        return false;
    }

    available_ = true;
    detail_ = api.detail;
    spdlog::info("reflex: {} ready, nvapi64.dll loaded", detail_);
    return true;
}

bool ReflexSession::available() const
{
    return available_;
}

bool ReflexSession::failed() const
{
    return failed_;
}

bool ReflexSession::stalled() const
{
    return stalled_;
}

core::ReflexMode ReflexSession::mode() const
{
    return mode_;
}

const std::string& ReflexSession::detail() const
{
    return detail_;
}

void ReflexSession::configure(core::ReflexMode mode, ID3D12Device* device)
{
    mode_ = mode;

    if (!available_ || failed_ || device == nullptr)
    {
        return;
    }

    const bool unchanged = configured_ && device_ == device && appliedMode_ == mode;
    device_ = device;

    if (unchanged)
    {
        return;
    }

    if (!applySleepMode(mode))
    {
        failed_ = true;
        return;
    }

    configured_ = true;
    // A new request deserves a fresh run of the detector and the report.
    stalled_ = false;
    stallDetector_ = {};
    latency_ = {};
    framesMarked_ = 0;
    nextPoll_ = std::chrono::steady_clock::now();
}

std::uint64_t ReflexSession::beginSourceFrame(std::uint32_t gpuWorkUs)
{
    const std::uint64_t frameId = nextFrameId_++;

    if (!available_ || failed_ || device_ == nullptr)
    {
        return frameId;
    }

    const auto started = std::chrono::steady_clock::now();
    const NvAPI_Status status = nvapi().sleep(device_);
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - started)
                             .count();

    if (status != NVAPI_OK)
    {
        failed_ = true;
        (void)applySleepMode(core::ReflexMode::Off);
        spdlog::error("reflex: NvAPI_D3D_Sleep failed: {}", statusText(nvapi(), status));
        return frameId;
    }

    const auto elapsedUs = static_cast<std::uint32_t>(elapsed);

    ++sleepCount_;
    lastSleepMilliseconds_ = static_cast<double>(elapsedUs) / 1000.0;
    addSleepSample(lastSleepMilliseconds_);

    const bool lowLatencyRequested = mode_ != core::ReflexMode::Off;

    if (lowLatencyRequested && !stalled_ && stallDetector_.observe(elapsedUs, gpuWorkUs))
    {
        disableAfterStall(elapsedUs, gpuWorkUs);
    }

    return frameId;
}

void ReflexSession::mark(std::uint64_t frameId, LatencyMarker marker)
{
    if (!available_ || device_ == nullptr)
    {
        return;
    }

    NV_LATENCY_MARKER_PARAMS params{};
    params.version = static_cast<NvU32>(NV_LATENCY_MARKER_PARAMS_VER1);
    params.frameID = frameId;
    params.markerType = markerType(marker);

    const NvAPI_Status status = nvapi().setLatencyMarker(device_, &params);

    if (status != NVAPI_OK)
    {
        if (!markerFailureLogged_)
        {
            markerFailureLogged_ = true;
            spdlog::warn("reflex: NvAPI_D3D_SetLatencyMarker failed: {}",
                         statusText(nvapi(), status));
        }

        return;
    }

    ++markerSets_;

    if (marker == LatencyMarker::SimulationStart)
    {
        ++framesMarked_;
    }

    if (!markersStarted_)
    {
        markersStarted_ = true;
        spdlog::info(
            "reflex: first latency markers accepted (simulation, render submit and present "
            "pairs per source frame)");
    }
}

void ReflexSession::poll()
{
    if (!available_ || device_ == nullptr)
    {
        return;
    }

    const auto now = std::chrono::steady_clock::now();

    if (now < nextPoll_)
    {
        return;
    }

    nextPoll_ = now + kPollPeriod;

    NV_GET_SLEEP_STATUS_PARAMS status{};
    status.version = static_cast<NvU32>(NV_GET_SLEEP_STATUS_PARAMS_VER1);

    if (const NvAPI_Status result = nvapi().getSleepStatus(device_, &status);
        result == NVAPI_OK)
    {
        sleepStatus_ = {
            .valid = true,
            .lowLatency = status.bLowLatencyMode != 0,
            .driverSeesSleep = status.bUseGameSleep != 0,
            .fullscreenFlip = status.bFullscreenIFlip != 0,
            .sleepIntervalUs = status.sleepIntervalUs,
        };
    }
    else
    {
        sleepStatus_ = {};

        if (!sleepStatusFailureLogged_)
        {
            sleepStatusFailureLogged_ = true;
            spdlog::warn("reflex: NvAPI_D3D_GetSleepStatus failed: {}",
                         statusText(nvapi(), result));
        }
    }

    if (framesMarked_ < kLatencyWarmupFrames)
    {
        return;
    }

    NV_LATENCY_RESULT_PARAMS report{};
    report.version = static_cast<NvU32>(NV_LATENCY_RESULT_PARAMS_VER1);

    if (const NvAPI_Status result = nvapi().getLatency(device_, &report); result != NVAPI_OK)
    {
        if (!latencyFailureLogged_)
        {
            latencyFailureLogged_ = true;
            spdlog::warn("reflex: NvAPI_D3D_GetLatency failed: {}", statusText(nvapi(), result));
        }

        return;
    }

    const auto& frame = report.frameReport[63];

    if (frame.frameID == 0 || frame.simStartTime == 0 || frame.gpuRenderEndTime == 0)
    {
        return;
    }

    latency_ = {
        .valid = true,
        .frameId = frame.frameID,
        .totalMilliseconds = microsecondsBetween(frame.gpuRenderEndTime, frame.simStartTime),
        .simulationMilliseconds = microsecondsBetween(frame.simEndTime, frame.simStartTime),
        .renderSubmitMilliseconds =
            microsecondsBetween(frame.renderSubmitEndTime, frame.renderSubmitStartTime),
        .presentMilliseconds = microsecondsBetween(frame.presentEndTime, frame.presentStartTime),
        .driverMilliseconds = microsecondsBetween(frame.driverEndTime, frame.driverStartTime),
        .queueMilliseconds =
            microsecondsBetween(frame.osRenderQueueEndTime, frame.osRenderQueueStartTime),
        .gpuMilliseconds = microsecondsBetween(frame.gpuRenderEndTime, frame.gpuRenderStartTime),
    };
}

const ReflexLatency& ReflexSession::latency() const
{
    return latency_;
}

const ReflexSleepStatus& ReflexSession::sleepStatus() const
{
    return sleepStatus_;
}

std::uint64_t ReflexSession::sleepCount() const
{
    return sleepCount_;
}

std::uint64_t ReflexSession::markerSets() const
{
    return markerSets_;
}

double ReflexSession::lastSleepMilliseconds() const
{
    return lastSleepMilliseconds_;
}

double ReflexSession::meanSleepMilliseconds() const
{
    if (sleepWindowCount_ == 0)
    {
        return 0.0;
    }

    double total = 0.0;

    for (std::size_t index = 0; index < sleepWindowCount_; ++index)
    {
        total += sleepWindow_.at(index);
    }

    return total / static_cast<double>(sleepWindowCount_);
}

double ReflexSession::maxSleepMilliseconds() const
{
    double longest = 0.0;

    for (std::size_t index = 0; index < sleepWindowCount_; ++index)
    {
        longest = std::max(longest, sleepWindow_.at(index));
    }

    return longest;
}

void ReflexSession::logSummary() const
{
    if (!available_)
    {
        spdlog::info("reflex: unavailable ({})", detail_);
        return;
    }

    std::string latencyText = "no valid report yet";

    if (latency_.valid)
    {
        latencyText = std::format(
            "frame {} total {:.1f} ms (GPU {:.1f}, render {:.1f}, present {:.1f}, driver {:.1f}, "
            "queue {:.1f})",
            latency_.frameId, latency_.totalMilliseconds, latency_.gpuMilliseconds,
            latency_.renderSubmitMilliseconds, latency_.presentMilliseconds,
            latency_.driverMilliseconds, latency_.queueMilliseconds);
    }

    spdlog::info("reflex: mode={} sleeps={} (one per source frame), markers={} (six per source "
                 "frame); sleep mean {:.3f} ms, max {:.3f} ms; effective lowLatency={} "
                 "driverSeesSleep={}{}; latency {}",
                 reflexModeName(mode_), sleepCount_, markerSets_, meanSleepMilliseconds(),
                 maxSleepMilliseconds(), sleepStatus_.lowLatency ? 1 : 0,
                 sleepStatus_.driverSeesSleep ? 1 : 0, failed_ ? " (sleep failed)" : "",
                 latencyText);
}

void ReflexSession::shutdown()
{
    if (available_ && configured_ && device_ != nullptr)
    {
        (void)applySleepMode(core::ReflexMode::Off);
    }

    device_ = nullptr;
    configured_ = false;
}

bool ReflexSession::applySleepMode(core::ReflexMode mode)
{
    NV_SET_SLEEP_MODE_PARAMS params{};
    params.version = static_cast<NvU32>(NV_SET_SLEEP_MODE_PARAMS_VER1);
    params.bLowLatencyMode = static_cast<NvBool>(mode != core::ReflexMode::Off);
    params.bLowLatencyBoost = static_cast<NvBool>(mode == core::ReflexMode::Boost);
    params.minimumIntervalUs = 0;
    params.bUseMarkersToOptimize = 0;

    const NvAPI_Status status = nvapi().setSleepMode(device_, &params);

    if (status != NVAPI_OK)
    {
        spdlog::error("reflex: NvAPI_D3D_SetSleepMode mode={} failed: {}", reflexModeName(mode),
                      statusText(nvapi(), status));
        return false;
    }

    appliedMode_ = mode;
    spdlog::info("reflex: NvAPI_D3D_SetSleepMode mode={} status={}, lowLatency={} boost={} "
                 "intervalUs=0",
                 reflexModeName(mode), statusText(nvapi(), status),
                 mode != core::ReflexMode::Off ? 1 : 0,
                 mode == core::ReflexMode::Boost ? 1 : 0);
    return true;
}

void ReflexSession::addSleepSample(double milliseconds)
{
    sleepWindow_.at(sleepWindowNext_) = milliseconds;
    sleepWindowNext_ = (sleepWindowNext_ + 1) % sleepWindow_.size();

    if (sleepWindowCount_ < sleepWindow_.size())
    {
        ++sleepWindowCount_;
    }
}

void ReflexSession::disableAfterStall(std::uint32_t sleepUs, std::uint32_t gpuWorkUs)
{
    stalled_ = true;
    (void)applySleepMode(core::ReflexMode::Off);

    spdlog::warn("reflex: sleep of {} us exceeds the frame's GPU work ({} us, with a {} us floor) "
                 "in {} of the last {} frames; low latency disabled for this stream",
                 sleepUs, gpuWorkUs, SleepStallDetector::kFloorUs, SleepStallDetector::kThreshold,
                 SleepStallDetector::kWindow);
}

} // namespace lynx::render
