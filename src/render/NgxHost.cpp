#include "render/NgxHost.h"

#include "core/Paths.h"
#include "render/AdapterSelection.h"
#include "render/GpuDevice.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <utility>

#include <spdlog/spdlog.h>

#include <windows.h>

#include <d3d12.h>
#include <nvapi.h>

#ifdef MFPLAY_HAVE_NGX
#pragma warning(push, 0)
#include <nvsdk_ngx.h>
#pragma warning(pop)
#endif

namespace mfplay::render
{

namespace
{

// A GUID-shaped id, which the NGX loader requires; MFPlay has no
// NVIDIA-issued application id and uses the custom-project path.
constexpr const char* kProjectId = "cbd9a4b0-3c4f-4a3d-9d9f-7b2a5e6f1c88";
constexpr const char* kEngineVersion = "0.1.0";

constexpr NvU32 kGetErrorMessageId = 0x6C2D048C;
constexpr NvU32 kEnumPhysicalGpusId = 0xE5AC921F;
constexpr NvU32 kGetArchInfoId = 0xD8265D24;

using QueryInterfaceFn = void*(__cdecl*)(NvU32);

struct Nvapi
{
    HMODULE module = nullptr;
    QueryInterfaceFn query = nullptr;
    decltype(&NvAPI_GetErrorMessage) errorMessage = nullptr;
    decltype(&NvAPI_EnumPhysicalGPUs) enumPhysicalGpus = nullptr;
    decltype(&NvAPI_GPU_GetArchInfo) getArchInfo = nullptr;
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

    api.errorMessage =
        reinterpret_cast<decltype(&NvAPI_GetErrorMessage)>(api.query(kGetErrorMessageId));
    api.enumPhysicalGpus =
        reinterpret_cast<decltype(&NvAPI_EnumPhysicalGPUs)>(api.query(kEnumPhysicalGpusId));
    api.getArchInfo =
        reinterpret_cast<decltype(&NvAPI_GPU_GetArchInfo)>(api.query(kGetArchInfoId));

    const auto initialize =
        reinterpret_cast<decltype(&NvAPI_Initialize)>(api.query(0x0150E828));

    if (initialize == nullptr)
    {
        api.detail = "nvapi64.dll has no NvAPI_Initialize";
        return api;
    }

    api.status = initialize();

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

std::expected<std::uint32_t, std::string> gpuArchitecture(const Nvapi& api)
{
    if (api.enumPhysicalGpus == nullptr || api.getArchInfo == nullptr)
    {
        return std::unexpected("nvapi64.dll has no NvAPI_EnumPhysicalGPUs/NvAPI_GPU_GetArchInfo");
    }

    NvPhysicalGpuHandle handles[NVAPI_MAX_PHYSICAL_GPUS]{};
    NvU32 count = 0;
    const NvAPI_Status enumStatus = api.enumPhysicalGpus(handles, &count);

    if (enumStatus != NVAPI_OK)
    {
        return std::unexpected(std::format("NvAPI_EnumPhysicalGPUs: {}",
                                           statusText(api, enumStatus)));
    }

    if (count == 0)
    {
        return std::unexpected("the NVIDIA driver enumerates no physical GPU");
    }

    NvU32 newest = 0;
    std::string failure;

    for (NvU32 index = 0; index < count; ++index)
    {
        NV_GPU_ARCH_INFO architecture{};
        architecture.version = NV_GPU_ARCH_INFO_VER;
        const NvAPI_Status status = api.getArchInfo(handles[index], &architecture);

        if (status != NVAPI_OK)
        {
            failure = statusText(api, status);
            continue;
        }

        newest = std::max(newest, architecture.architecture);
    }

    if (newest == 0)
    {
        return std::unexpected(
            std::format("NvAPI_GPU_GetArchInfo: {}", failure.empty() ? "unknown" : failure));
    }

    return newest;
}

std::filesystem::path executableDirectory()
{
    std::wstring buffer(MAX_PATH, L'\0');

    while (true)
    {
        const DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

        if (length == 0)
        {
            return {};
        }

        if (length < buffer.size())
        {
            buffer.resize(length);
            break;
        }

        buffer.resize(buffer.size() * 2);
    }

    return std::filesystem::path(buffer).parent_path();
}

bool fileExists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

#ifdef MFPLAY_HAVE_NGX

bool ngxFailed(NVSDK_NGX_Result result)
{
    return (static_cast<std::uint32_t>(result) & 0xFFF00000U) == 0xBAD00000U;
}

void NVSDK_CONV ngxLog(const char* message, NVSDK_NGX_Logging_Level level,
                        NVSDK_NGX_Feature source)
{
    spdlog::debug("NGX[{}:{}]: {}", static_cast<int>(source), static_cast<int>(level),
                  message != nullptr ? message : "");
}

std::filesystem::path applicationDataPath()
{
    if (const auto dataDir = core::localDataDir(); dataDir)
    {
        const std::filesystem::path path = *dataDir / "ngx";
        std::error_code error;
        std::filesystem::create_directories(path, error);

        if (!error)
        {
            return path;
        }
    }

    return executableDirectory();
}

// The loader's Init call can raise on a device the driver can't handle; the
// caller turns the exception into a regular failure.
__declspec(noinline) NVSDK_NGX_Result callInitWithProjectId(
    const char* projectId, NVSDK_NGX_EngineType engineType, const char* engineVersion,
    const wchar_t* dataPath, ID3D12Device* device, const NVSDK_NGX_FeatureCommonInfo* featureInfo)
{
    NVSDK_NGX_Result result = NVSDK_NGX_Result_Fail;

    __try
    {
        result = NVSDK_NGX_D3D12_Init_with_ProjectID(projectId, engineType, engineVersion,
                                                      dataPath, device, featureInfo,
                                                      NVSDK_NGX_Version_API);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        result = NVSDK_NGX_Result_FAIL_PlatformError;
    }

    return result;
}

__declspec(noinline) NVSDK_NGX_Result callShutdown(ID3D12Device* device)
{
    NVSDK_NGX_Result result = NVSDK_NGX_Result_Fail;

    __try
    {
        result = NVSDK_NGX_D3D12_Shutdown1(device);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        result = NVSDK_NGX_Result_FAIL_PlatformError;
    }

    return result;
}

#endif

} // namespace

std::string ngxResultText(std::uint32_t result)
{
#ifdef MFPLAY_HAVE_NGX
    const wchar_t* const text = GetNGXResultAsString(static_cast<NVSDK_NGX_Result>(result));

    if (text != nullptr)
    {
        return core::toUtf8(text);
    }
#else
    (void)result;
#endif

    return "unknown NGX result";
}

bool dlssArchitectureSupported(std::uint32_t architecture)
{
    return architecture >= kAdaArchitecture;
}

DlssAvailability probeDlss()
{
    const auto adapters = listAdapters();

    if (!adapters)
    {
        return {
            .available = false,
            .reason = DlssUnavailable::NoNvidiaGpu,
            .detail = adapters.error(),
        };
    }

    const auto chosen = selectAdapter(*adapters);

    if (!chosen)
    {
        return {
            .available = false,
            .reason = DlssUnavailable::NoNvidiaGpu,
            .detail = "no hardware adapter can render the stream",
        };
    }

    const AdapterInfo& adapter = adapters->at(*chosen);

    if (adapter.vendorId != kNvidiaVendorId)
    {
        return {
            .available = false,
            .reason = DlssUnavailable::NoNvidiaGpu,
            .detail = std::format("the stream runs on {} (vendor 0x{:04X})", adapter.name,
                                  adapter.vendorId),
        };
    }

    const Nvapi& api = nvapi();

    if (api.module == nullptr || api.status != NVAPI_OK)
    {
        return {
            .available = false,
            .reason = DlssUnavailable::ArchitectureUnknown,
            .detail = api.detail,
        };
    }

    const auto architecture = gpuArchitecture(api);

    if (!architecture)
    {
        return {
            .available = false,
            .reason = DlssUnavailable::ArchitectureUnknown,
            .detail = architecture.error(),
        };
    }

    if (!dlssArchitectureSupported(*architecture))
    {
        return {
            .available = false,
            .reason = DlssUnavailable::GpuTooOld,
            .detail = std::format("GPU architecture 0x{:X} is older than Ada", *architecture),
        };
    }

    if (!fileExists(executableDirectory() / "nvngx_dlss.dll"))
    {
        return {
            .available = false,
            .reason = DlssUnavailable::NoDlssRuntime,
            .detail = "nvngx_dlss.dll isn't next to the executable",
        };
    }

    return {
        .available = true,
        .reason = DlssUnavailable::None,
        .detail = std::format("{} with nvngx_dlss.dll", adapter.name),
    };
}

std::expected<NgxHost, std::string> NgxHost::create(ID3D12Device& device)
{
#ifdef MFPLAY_HAVE_NGX
    const std::filesystem::path dataPath = applicationDataPath();
    const std::filesystem::path executable = executableDirectory();

    if (executable.empty())
    {
        return std::unexpected("cannot find the executable's folder");
    }

    const std::wstring dataPathText = dataPath.wstring();
    const std::wstring executableText = executable.wstring();
    const wchar_t* paths[] = {executableText.c_str()};
    NVSDK_NGX_FeatureCommonInfo common{};
    common.LoggingInfo.LoggingCallback = ngxLog;
    common.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_ON;
    common.LoggingInfo.DisableOtherLoggingSinks = true;
    common.PathListInfo.Path = paths;
    common.PathListInfo.Length = 1U;

    const NVSDK_NGX_Result result = callInitWithProjectId(
        kProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, kEngineVersion, dataPathText.c_str(), &device,
        &common);

    if (ngxFailed(result))
    {
        return std::unexpected(
            std::format("NVSDK_NGX_D3D12_Init_with_ProjectID: {}",
                        ngxResultText(static_cast<std::uint32_t>(result))));
    }

    NgxHost host;
    host.device_ = &device;
    host.initialized_ = true;
    host.description_ = std::format("NGX 310.9.16, project {}", kProjectId);
    spdlog::info("NGX: initialized with the stream's device ({}), app data '{}'", host.description_,
                 dataPath.string());

    return host;
#else
    (void)device;
    return std::unexpected("the NGX SDK wasn't fetched (scripts/fetch-ngx.ps1)");
#endif
}

NgxHost::NgxHost(NgxHost&& other) noexcept
    : device_(std::move(other.device_)), description_(std::move(other.description_)),
      initialized_(std::exchange(other.initialized_, false))
{
}

NgxHost& NgxHost::operator=(NgxHost&& other) noexcept
{
    if (this != &other)
    {
        shutdown();
        device_ = std::move(other.device_);
        description_ = std::move(other.description_);
        initialized_ = std::exchange(other.initialized_, false);
    }

    return *this;
}

NgxHost::~NgxHost()
{
    shutdown();
}

void NgxHost::shutdown()
{
#if defined(MFPLAY_HAVE_NGX)
    if (!initialized_ || !device_)
    {
        return;
    }

    if (const NVSDK_NGX_Result result = callShutdown(device_.Get()); ngxFailed(result))
    {
        spdlog::warn("NGX: shutdown failed: {}",
                     ngxResultText(static_cast<std::uint32_t>(result)));
    }
#endif

    device_.Reset();
    initialized_ = false;
    description_.clear();
}

ID3D12Device* NgxHost::device() const
{
    return device_.Get();
}

bool NgxHost::initialized() const
{
    return initialized_;
}

const std::string& NgxHost::description() const
{
    return description_;
}

std::expected<NVSDK_NGX_Parameter*, std::string> NgxHost::allocateParameters() const
{
#if !defined(MFPLAY_HAVE_NGX)
    return std::unexpected("the NGX SDK wasn't fetched (scripts/fetch-ngx.ps1)");
#else
    if (!initialized_)
    {
        return std::unexpected("NGX isn't initialized");
    }

    NVSDK_NGX_Parameter* parameters = nullptr;
    const NVSDK_NGX_Result result = NVSDK_NGX_D3D12_AllocateParameters(&parameters);

    if (ngxFailed(result) || parameters == nullptr)
    {
        return std::unexpected(std::format("NVSDK_NGX_D3D12_AllocateParameters: {}",
                                           ngxResultText(static_cast<std::uint32_t>(result))));
    }

    return parameters;
#endif
}

std::expected<NVSDK_NGX_Parameter*, std::string> NgxHost::capabilityParameters() const
{
#if !defined(MFPLAY_HAVE_NGX)
    return std::unexpected("the NGX SDK wasn't fetched (scripts/fetch-ngx.ps1)");
#else
    if (!initialized_)
    {
        return std::unexpected("NGX isn't initialized");
    }

    NVSDK_NGX_Parameter* parameters = nullptr;
    const NVSDK_NGX_Result result = NVSDK_NGX_D3D12_GetCapabilityParameters(&parameters);

    if (ngxFailed(result) || parameters == nullptr)
    {
        return std::unexpected(std::format("NVSDK_NGX_D3D12_GetCapabilityParameters: {}",
                                           ngxResultText(static_cast<std::uint32_t>(result))));
    }

    return parameters;
#endif
}

void NgxHost::destroyParameters(NVSDK_NGX_Parameter* parameters)
{
#if defined(MFPLAY_HAVE_NGX)
    if (parameters != nullptr)
    {
        NVSDK_NGX_D3D12_DestroyParameters(parameters);
    }
#else
    (void)parameters;
#endif
}

} // namespace mfplay::render
