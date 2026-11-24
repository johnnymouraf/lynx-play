#include "render/FrameGeneration.h"

#include "core/Windows.h"
#include "render/AdapterSelection.h"
#include "render/GpuDevice.h"

#include <array>
#include <filesystem>
#include <format>
#include <limits>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>

#if defined(MFPLAY_HAVE_NGX)
#pragma warning(push, 0)
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_defs_dlssg.h>
#include <nvsdk_ngx_helpers_dlssg_d3d.h>
#include <nvsdk_ngx_params_dlssg.h>
#pragma warning(pop)
#endif

namespace mfplay::render
{

namespace
{

using core::describeFailure;
using Microsoft::WRL::ComPtr;

#if defined(MFPLAY_HAVE_NGX)
__declspec(noinline) NVSDK_NGX_Result callCreateDlssg(
    ID3D12GraphicsCommandList* commandList, NVSDK_NGX_Handle** handle,
    NVSDK_NGX_Parameter* parameters, NVSDK_NGX_DLSSG_Create_Params* createParameters)
{
    NVSDK_NGX_Result result = NVSDK_NGX_Result_Fail;

    __try
    {
        result = NGX_D3D12_CREATE_DLSSG(commandList, 1, 1, handle, parameters, createParameters);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        result = NVSDK_NGX_Result_FAIL_PlatformError;
    }

    return result;
}

__declspec(noinline) NVSDK_NGX_Result callEvaluateDlssg(
    ID3D12GraphicsCommandList* commandList, NVSDK_NGX_Handle* handle,
    NVSDK_NGX_Parameter* parameters, NVSDK_NGX_D3D12_DLSSG_Eval_Params* resources,
    NVSDK_NGX_DLSSG_Opt_Eval_Params* options)
{
    NVSDK_NGX_Result result = NVSDK_NGX_Result_Fail;

    __try
    {
        result = NGX_D3D12_EVALUATE_DLSSG(commandList, handle, parameters, resources, options);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        result = NVSDK_NGX_Result_FAIL_PlatformError;
    }

    return result;
}

__declspec(noinline) NVSDK_NGX_Result callReleaseDlssg(NVSDK_NGX_Handle* handle)
{
    NVSDK_NGX_Result result = NVSDK_NGX_Result_Fail;

    __try
    {
        result = NVSDK_NGX_D3D12_ReleaseFeature(handle);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        result = NVSDK_NGX_Result_FAIL_PlatformError;
    }

    return result;
}

constexpr float kConstantDepth = 0.9F;
// The sign of mvecScale, which turns pixel vectors into DLSS-G's normalized
// ones (vector * mvecScale).
//
// The vectors here point from a pixel of the current picture to where it was
// in the previous one: the optical flow runs with the new picture as
// inputFrame and the one before it as referenceFrame, and NVIDIA's header
// defines the forward flow as the position change from inputFrame to
// referenceFrame; FlowDensify.hlsl keeps that direction and only rescales.
//
// NVIDIA's DLSS guide (3.6) asks for exactly that, current position plus
// vector is the previous position, and Streamline's DLSS-G guide sets
// mvecScale = {1/width, 1/height} for pixel vectors, which would make this
// +1. The sign stays -1 because the reference app (Veyra) ships DLSS-G with
// current-to-previous pixel vectors and -1/W, and the NGX header says only
// "normalize". The sources disagree and nothing here has run on a pan, so
// the sign is unconfirmed: a generated picture that doubles or rubber-bands
// on a steady pan means it should be +1.
constexpr float kMotionVectorScaleSign = -1.0F;

std::filesystem::path executableDirectory()
{
    std::array<wchar_t, MAX_PATH> path{};
    const DWORD length =
        GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));

    if (length == 0 || length >= path.size())
    {
        return {};
    }

    return std::filesystem::path(std::wstring_view(path.data(), length)).parent_path();
}
#endif

std::optional<DWORD> hagsMode()
{
    HKEY key = nullptr;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", 0, KEY_READ,
                      &key) != ERROR_SUCCESS)
    {
        return std::nullopt;
    }

    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LSTATUS status =
        RegQueryValueExW(key, L"HwSchMode", nullptr, &type, reinterpret_cast<LPBYTE>(&value),
                         &size);
    RegCloseKey(key);

    if (status != ERROR_SUCCESS || type != REG_DWORD)
    {
        return std::nullopt;
    }

    return value;
}

#if defined(MFPLAY_HAVE_NGX)
D3D12_RESOURCE_DESC textureDescription(SurfaceSize extent, DXGI_FORMAT format,
                                       D3D12_RESOURCE_FLAGS flags)
{
    return {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = extent.width,
        .Height = extent.height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = format,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = flags,
    };
}

D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;

    return barrier;
}

D3D12_RESOURCE_BARRIER unorderedAccessBarrier()
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = nullptr;

    return barrier;
}

std::string resultText(NVSDK_NGX_Result result)
{
    switch (result)
    {
    case NVSDK_NGX_Result_Success:
        return "success";
    case NVSDK_NGX_Result_FAIL_FeatureNotSupported:
        return "feature not supported";
    case NVSDK_NGX_Result_FAIL_InvalidParameter:
        return "invalid parameter";
    case NVSDK_NGX_Result_FAIL_OutOfDate:
        return "driver out of date";
    case NVSDK_NGX_Result_FAIL_UnableToInitializeFeature:
        return "unable to initialize the feature";
    case NVSDK_NGX_Result_FAIL_UnsupportedInputFormat:
        return "unsupported input format";
    case NVSDK_NGX_Result_FAIL_RWFlagMissing:
        return "output isn't writable";
    case NVSDK_NGX_Result_FAIL_OutOfGPUMemory:
        return "out of GPU memory";
    default:
        break;
    }

    return std::format("0x{:08X}", static_cast<unsigned>(result));
}

// A stationary synthetic camera: all the motion is in the vector picture,
// but the runtime still wants consistent matrices and a lens.
NVSDK_NGX_DLSSG_Opt_Eval_Params evaluationParameters(SurfaceSize extent, bool hdr,
                                                      bool reset)
{
    NVSDK_NGX_DLSSG_Opt_Eval_Params parameters{};
    parameters.multiFrameCount = 1;
    parameters.multiFrameIndex = 1;

    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            const float value = row == column ? 1.0F : 0.0F;
            parameters.cameraViewToClip[row][column] = value;
            parameters.clipToCameraView[row][column] = value;
            parameters.clipToLensClip[row][column] = value;
            parameters.clipToPrevClip[row][column] = value;
            parameters.prevClipToClip[row][column] = value;
        }
    }

    parameters.jitterOffset[0] = 0.0F;
    parameters.jitterOffset[1] = 0.0F;
    parameters.mvecScale[0] = kMotionVectorScaleSign / static_cast<float>(extent.width);
    parameters.mvecScale[1] = kMotionVectorScaleSign / static_cast<float>(extent.height);
    parameters.cameraNear = 0.1F;
    parameters.cameraFar = 1000.0F;
    parameters.cameraFOV = 1.5707963F;
    parameters.cameraAspectRatio =
        static_cast<float>(extent.width) / static_cast<float>(extent.height);
    parameters.cameraUp[1] = 1.0F;
    parameters.cameraRight[0] = 1.0F;
    parameters.cameraFwd[2] = 1.0F;

    const float projectionX = 1.0F / parameters.cameraAspectRatio;
    const float projectionZ = parameters.cameraFar / (parameters.cameraFar - parameters.cameraNear);
    const float projectionW = -parameters.cameraNear * projectionZ;
    parameters.cameraViewToClip[0][0] = projectionX;
    parameters.cameraViewToClip[2][2] = projectionZ;
    parameters.cameraViewToClip[2][3] = 1.0F;
    parameters.cameraViewToClip[3][2] = projectionW;
    parameters.cameraViewToClip[3][3] = 0.0F;
    parameters.clipToCameraView[0][0] = 1.0F / projectionX;
    parameters.clipToCameraView[2][2] = 0.0F;
    parameters.clipToCameraView[2][3] = 1.0F / projectionW;
    parameters.clipToCameraView[3][2] = 1.0F;
    parameters.clipToCameraView[3][3] = -projectionZ / projectionW;

    parameters.colorBuffersHDR = hdr;
    parameters.depthInverted = false;
    parameters.cameraMotionIncluded = true;
    parameters.reset = reset;
    parameters.automodeOverrideReset = false;
    parameters.notRenderingGameFrames = false;
    parameters.orthoProjection = false;
    parameters.motionVectorsDilated = false;
    parameters.motionVectorsInvalidValue = std::numeric_limits<float>::max();
    parameters.menuDetectionEnabled = false;
    parameters.backbufferSubrectSize = {extent.width, extent.height};
    parameters.outputInterpSubrectSize = {extent.width, extent.height};
    parameters.mvecsSubrectSize = {extent.width, extent.height};
    parameters.depthSubrectSize = {extent.width, extent.height};

    return parameters;
}
#endif

} // namespace

struct FrameGeneration::Impl
{
    ~Impl()
    {
        release();
    }

    void release()
    {
#if defined(MFPLAY_HAVE_NGX)
        if (handle != nullptr)
        {
            const NVSDK_NGX_Result result = callReleaseDlssg(handle);

            if (result != NVSDK_NGX_Result_Success)
            {
                spdlog::warn("ngx: ReleaseFeature: {}", resultText(result));
            }

            handle = nullptr;
        }

        if (parameters != nullptr)
        {
            NgxHost::destroyParameters(parameters);
            parameters = nullptr;
        }
#endif
        generated.Reset();
        depth.Reset();
        depthViews.Reset();
        extent = {};
        format = DXGI_FORMAT_UNKNOWN;
        description.clear();
    }

    NgxHost* host = nullptr;
    ComPtr<ID3D12Device> device;
    NVSDK_NGX_Parameter* parameters = nullptr;
    NVSDK_NGX_Handle* handle = nullptr;
    ComPtr<ID3D12Resource> depth;
    ComPtr<ID3D12DescriptorHeap> depthViews;
    ComPtr<ID3D12Resource> generated;
    D3D12_RESOURCE_STATES generatedState = D3D12_RESOURCE_STATE_COMMON;
    SurfaceSize extent;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    std::string description;
    std::uint64_t evaluationCount = 0;
    bool failed = false;
};

namespace
{

FrameGenerationAvailability computeProbe()
{
    FrameGenerationAvailability availability;

    const std::optional<DWORD> hags = hagsMode();
    // Windows 11 can leave HwSchMode absent when hardware GPU scheduling is
    // the default; only an explicit 1 (off) is a refusal. The runtime's own
    // capability below is the final word.
    availability.hagsOn = !hags || *hags != 1;

#if !defined(MFPLAY_HAVE_NGX)
    availability.reason = FrameGenerationUnavailable::RuntimeMissing;
    availability.detail = "the build has no NVIDIA NGX SDK";
    return availability;
#else
    const std::filesystem::path executable = executableDirectory();

    if (executable.empty() || !std::filesystem::exists(executable / "nvngx_dlssg.dll"))
    {
        availability.reason = FrameGenerationUnavailable::RuntimeMissing;
        availability.detail = "nvngx_dlssg.dll isn't next to the executable";
        return availability;
    }

    if (!availability.hagsOn)
    {
        availability.reason = FrameGenerationUnavailable::HagsOff;
        availability.detail = std::format("HwSchMode={} (hardware GPU scheduling is off)",
                                          hags.value_or(0));
        return availability;
    }

    const auto stream = findStreamAdapter();

    if (!stream)
    {
        availability.reason = FrameGenerationUnavailable::DriverFailed;
        availability.detail = stream.error();
        return availability;
    }

    if (stream->info.vendorId != kNvidiaVendorId || stream->info.isSoftware)
    {
        availability.reason = FrameGenerationUnavailable::NoNvidiaGpu;
        availability.detail = std::format("the stream renders on {}", stream->info.name);
        return availability;
    }

    ComPtr<ID3D12Device> device;

    if (const HRESULT result = D3D12CreateDevice(stream->adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                                  IID_PPV_ARGS(&device));
        FAILED(result))
    {
        availability.reason = FrameGenerationUnavailable::DriverFailed;
        availability.detail = describeFailure("D3D12CreateDevice", result);
        return availability;
    }

    auto host = NgxHost::create(*device.Get());

    if (!host)
    {
        availability.reason = FrameGenerationUnavailable::DriverFailed;
        availability.detail = host.error();
        return availability;
    }

    const auto capabilityParameters = host->capabilityParameters();

    if (!capabilityParameters)
    {
        availability.reason = FrameGenerationUnavailable::DriverFailed;
        availability.detail = capabilityParameters.error();
        return availability;
    }

    NVSDK_NGX_Parameter* const parameters = *capabilityParameters;

    int available = 0;
    NVSDK_NGX_Result availableResult =
        parameters->Get(NVSDK_NGX_Parameter_FrameGeneration_Available, &available);

    if (availableResult != NVSDK_NGX_Result_Success)
    {
        unsigned long long wideValue = 0;
        availableResult =
            parameters->Get(NVSDK_NGX_Parameter_FrameGeneration_Available, &wideValue);
        available = wideValue != 0 ? 1 : 0;
    }

    int needsUpdatedDriver = 0;
    parameters->Get(NVSDK_NGX_Parameter_FrameGeneration_NeedsUpdatedDriver,
                     &needsUpdatedDriver);

    int minimumMajor = 0;
    int minimumMinor = 0;
    parameters->Get(NVSDK_NGX_Parameter_FrameGeneration_MinDriverVersionMajor, &minimumMajor);
    parameters->Get(NVSDK_NGX_Parameter_FrameGeneration_MinDriverVersionMinor, &minimumMinor);

    unsigned int multiFrameCountMax = 0;

    if (parameters->Get(NVSDK_NGX_DLSSG_Parameter_MultiFrameCountMax, &multiFrameCountMax) !=
        NVSDK_NGX_Result_Success)
    {
        int narrowValue = 0;
        parameters->Get(NVSDK_NGX_DLSSG_Parameter_MultiFrameCountMax, &narrowValue);
        multiFrameCountMax = narrowValue > 0 ? static_cast<unsigned int>(narrowValue) : 0U;
    }

    NgxHost::destroyParameters(parameters);

    availability.multiFrameCountMax = multiFrameCountMax;

    if (availableResult != NVSDK_NGX_Result_Success || available == 0)
    {
        availability.reason = needsUpdatedDriver != 0
                                  ? FrameGenerationUnavailable::DriverTooOld
                                  : FrameGenerationUnavailable::NotAvailable;
        availability.detail = needsUpdatedDriver != 0
                                  ? std::format("the driver is older than {}.{}", minimumMajor,
                                                minimumMinor)
                                  : "FrameGeneration.Available is 0 (below Ada)";
        return availability;
    }

    availability.available = true;
    availability.reason = FrameGenerationUnavailable::None;
    availability.detail =
        std::format("DLSS-G on {} (multi-frame max {}), {}",
                    stream->info.name, multiFrameCountMax,
                    hags ? std::format("HwSchMode={}", *hags)
                         : std::string("HwSchMode absent (assuming hardware GPU scheduling)"));
    return availability;
#endif
}

} // namespace

FrameGenerationAvailability FrameGeneration::probe()
{
    static std::mutex mutex;
    static std::optional<FrameGenerationAvailability> cached;
    const std::lock_guard lock(mutex);

    if (!cached)
    {
        cached = computeProbe();
    }

    return *cached;
}

std::expected<FrameGeneration, std::string> FrameGeneration::create(
    NgxHost& host, ID3D12Device& device, ID3D12GraphicsCommandList& commandList,
    SurfaceSize extent, DXGI_FORMAT backbufferFormat)
{
    if (extent.width == 0 || extent.height == 0)
    {
        return std::unexpected("the back buffer has no size");
    }

#if !defined(MFPLAY_HAVE_NGX)
    return std::unexpected("the build has no NVIDIA NGX SDK");
#else
    auto impl = std::make_unique<Impl>();
    impl->host = &host;
    impl->device = &device;
    impl->extent = extent;
    impl->format = backbufferFormat;

    const D3D12_HEAP_PROPERTIES defaultHeap{.Type = D3D12_HEAP_TYPE_DEFAULT};

    const D3D12_RESOURCE_DESC generatedDescription =
        textureDescription(extent, backbufferFormat, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    if (const HRESULT result = device.CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &generatedDescription,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&impl->generated));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommittedResource (generated frame)",
                                               result));
    }

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_R32_FLOAT;
    clearValue.Color[0] = kConstantDepth;

    const D3D12_RESOURCE_DESC depthDescription = textureDescription(
        extent, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    if (const HRESULT result = device.CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &depthDescription,
            D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS(&impl->depth));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommittedResource (depth)", result));
    }

    const D3D12_DESCRIPTOR_HEAP_DESC viewHeapDescription{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = 1,
    };

    if (const HRESULT result =
            device.CreateDescriptorHeap(&viewHeapDescription, IID_PPV_ARGS(&impl->depthViews));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateDescriptorHeap (depth)", result));
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE depthView =
        impl->depthViews->GetCPUDescriptorHandleForHeapStart();
    device.CreateRenderTargetView(impl->depth.Get(), nullptr, depthView);
    commandList.ClearRenderTargetView(depthView, clearValue.Color, 0, nullptr);

    const auto depthToShader =
        transition(impl->depth.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList.ResourceBarrier(1, &depthToShader);

    const auto allocated = host.allocateParameters();

    if (!allocated)
    {
        return std::unexpected(allocated.error());
    }

    impl->parameters = *allocated;

    const auto alwaysProvided = static_cast<unsigned int>(
        NVSDK_NGX_DLSSG_ResourceFlags_Backbuffer | NVSDK_NGX_DLSSG_ResourceFlags_MVecs |
        NVSDK_NGX_DLSSG_ResourceFlags_Depth);
    const auto neverProvided = static_cast<unsigned int>(
        NVSDK_NGX_DLSSG_ResourceFlags_HUDLess | NVSDK_NGX_DLSSG_ResourceFlags_UI |
        NVSDK_NGX_DLSSG_ResourceFlags_UIAlpha |
        NVSDK_NGX_DLSSG_ResourceFlags_BidirectionalDistortionField);
    impl->parameters->Set(NVSDK_NGX_DLSSG_Parameter_ResourceAlwaysProvided_Flags,
                          alwaysProvided);
    impl->parameters->Set(NVSDK_NGX_DLSSG_Parameter_ResourceNeverProvided_Flags, neverProvided);

    NVSDK_NGX_DLSSG_Create_Params createParameters{};
    createParameters.Width = extent.width;
    createParameters.Height = extent.height;
    createParameters.NativeBackbufferFormat = static_cast<unsigned int>(backbufferFormat);
    createParameters.RenderWidth = extent.width;
    createParameters.RenderHeight = extent.height;
    createParameters.DynamicResolutionScaling = false;

    const NVSDK_NGX_Result result =
        callCreateDlssg(&commandList, &impl->handle, impl->parameters, &createParameters);

    if (result != NVSDK_NGX_Result_Success || impl->handle == nullptr)
    {
        unsigned long long initResult = 0;
        impl->parameters->Get(NVSDK_NGX_Parameter_FrameGeneration_FeatureInitResult,
                               &initResult);
        const std::string detail = std::format(
            "NGX_D3D12_CREATE_DLSSG: {} (feature init 0x{:08X})", resultText(result),
            static_cast<unsigned>(initResult));

        if (impl->handle != nullptr)
        {
            callReleaseDlssg(impl->handle);
            impl->handle = nullptr;
        }

        impl->release();
        return std::unexpected(detail);
    }

    impl->description = std::format("DLSS-G {}x{}", extent.width, extent.height);
    return FrameGeneration(std::move(impl));
#endif
}

FrameGeneration::FrameGeneration(std::unique_ptr<Impl> impl) : impl_(std::move(impl))
{
}

FrameGeneration::FrameGeneration(FrameGeneration&&) noexcept = default;
FrameGeneration& FrameGeneration::operator=(FrameGeneration&&) noexcept = default;
FrameGeneration::~FrameGeneration() = default;

bool FrameGeneration::configuredFor(SurfaceSize extent, DXGI_FORMAT backbufferFormat) const
{
    return impl_ != nullptr && impl_->handle != nullptr && impl_->extent == extent &&
           impl_->format == backbufferFormat;
}

ID3D12Resource* FrameGeneration::depth() const
{
    return impl_ != nullptr ? impl_->depth.Get() : nullptr;
}

ID3D12Resource* FrameGeneration::generated() const
{
    return impl_ != nullptr ? impl_->generated.Get() : nullptr;
}

SurfaceSize FrameGeneration::extent() const
{
    return impl_ != nullptr ? impl_->extent : SurfaceSize{};
}

DXGI_FORMAT FrameGeneration::format() const
{
    return impl_ != nullptr ? impl_->format : DXGI_FORMAT_UNKNOWN;
}

std::expected<void, std::string> FrameGeneration::evaluate(
    ID3D12GraphicsCommandList& commandList, ID3D12Resource& backbuffer,
    ID3D12Resource& motionVectors, bool reset, std::uint64_t frameId)
{
#if !defined(MFPLAY_HAVE_NGX)
    (void)commandList;
    (void)backbuffer;
    (void)motionVectors;
    (void)reset;
    (void)frameId;
    return std::unexpected("the build has no NVIDIA NGX SDK");
#else
    if (impl_ == nullptr || impl_->handle == nullptr || impl_->parameters == nullptr)
    {
        return std::unexpected("the DLSS-G feature isn't created");
    }

    if (impl_->failed)
    {
        return std::unexpected("DLSS-G failed earlier in this stream");
    }

    Impl& impl = *impl_;

    const auto generatedToUnorderedAccess =
        transition(impl.generated.Get(), impl.generatedState,
                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList.ResourceBarrier(1, &generatedToUnorderedAccess);

    NVSDK_NGX_D3D12_DLSSG_Eval_Params resources{};
    resources.pBackbuffer = &backbuffer;
    resources.pDepth = impl.depth.Get();
    resources.pMVecs = &motionVectors;
    resources.pOutputInterpFrame = impl.generated.Get();

    NVSDK_NGX_DLSSG_Opt_Eval_Params options =
        evaluationParameters(impl.extent, impl.format == DXGI_FORMAT_R10G10B10A2_UNORM, reset);
    impl.parameters->Set(NVSDK_NGX_DLSSG_Parameter_BackbufferFrameID,
                         static_cast<unsigned long long>(frameId));

    const NVSDK_NGX_Result result =
        callEvaluateDlssg(&commandList, impl.handle, impl.parameters, &resources, &options);

    if (result != NVSDK_NGX_Result_Success)
    {
        impl.failed = true;
        return std::unexpected(std::format("NGX_D3D12_EVALUATE_DLSSG: {}", resultText(result)));
    }

    const D3D12_RESOURCE_BARRIER dispatchBarrier = unorderedAccessBarrier();
    commandList.ResourceBarrier(1, &dispatchBarrier);

    const auto generatedToCopy =
        transition(impl.generated.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                   D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList.ResourceBarrier(1, &generatedToCopy);
    impl.generatedState = D3D12_RESOURCE_STATE_COPY_SOURCE;

    ++impl.evaluationCount;
    return {};
#endif
}

void FrameGeneration::generatedCopied()
{
    if (impl_ != nullptr)
    {
        impl_->generatedState = D3D12_RESOURCE_STATE_COMMON;
    }
}

const std::string& FrameGeneration::description() const
{
    static const std::string empty;
    return impl_ != nullptr ? impl_->description : empty;
}

std::uint64_t FrameGeneration::evaluations() const
{
    return impl_ != nullptr ? impl_->evaluationCount : 0;
}

bool FrameGeneration::failed() const
{
    return impl_ != nullptr && impl_->failed;
}

void FrameGeneration::release()
{
    if (impl_ != nullptr)
    {
        impl_->release();
    }
}

} // namespace mfplay::render
