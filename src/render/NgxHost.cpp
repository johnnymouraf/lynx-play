#include "render/NgxHost.h"

#include "core/Paths.h"

#include <array>
#include <filesystem>
#include <format>
#include <string_view>
#include <system_error>

#include <spdlog/spdlog.h>

#if defined(LYNX_HAVE_NGX)
#pragma warning(push, 0)
#include <nvsdk_ngx.h>
#pragma warning(pop)
#endif

namespace lynx::render
{

namespace
{

// The loader only accepts a GUID-shaped project id; the engine is a custom
// application without a NVIDIA-issued id.
constexpr const char* kProjectId = "3f6a2c1e-8b4d-4e7a-9c52-1d0f6a8b3e21";
constexpr const char* kEngineVersion = "0.1.0";

#if defined(LYNX_HAVE_NGX)
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

std::string resultText(NVSDK_NGX_Result result)
{
    return std::format("0x{:08X}", static_cast<unsigned>(result));
}
#endif

} // namespace

NgxHost::~NgxHost()
{
    shutdown();
}

std::expected<void, std::string> NgxHost::initialize(ID3D12Device& device)
{
#if !defined(LYNX_HAVE_NGX)
    (void)device;
    return std::unexpected("the build has no NVIDIA NGX SDK");
#else
    if (device_ != nullptr)
    {
        return {};
    }

    const auto dataDir = core::localDataDir();

    if (!dataDir)
    {
        return std::unexpected(dataDir.error());
    }

    const std::filesystem::path appData = *dataDir / "NGX";
    std::error_code error;
    std::filesystem::create_directories(appData, error);

    if (error)
    {
        return std::unexpected("cannot create " + appData.string() + ": " + error.message());
    }

    const std::filesystem::path executable = executableDirectory();

    if (executable.empty())
    {
        return std::unexpected("cannot find the executable's folder");
    }

    const std::wstring executablePath = executable.wstring();
    const std::wstring appDataPath = appData.wstring();
    const wchar_t* paths[] = {executablePath.c_str()};
    NVSDK_NGX_FeatureCommonInfo common{};
    common.PathListInfo.Path = paths;
    common.PathListInfo.Length = 1U;

    const NVSDK_NGX_Result result =
        callInitWithProjectId(kProjectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, kEngineVersion,
                              appDataPath.c_str(), &device, &common);

    if (result != NVSDK_NGX_Result_Success)
    {
        return std::unexpected("NVSDK_NGX_D3D12_Init_with_ProjectID: " + resultText(result));
    }

    device_ = &device;
    description_ = "NGX (DLSS runtime 310.9.16)";
    spdlog::info("ngx: initialized for device {}, app data '{}', features in '{}'",
                 static_cast<const void*>(&device), appData.string(), executable.string());
    return {};
#endif
}

void NgxHost::shutdown()
{
#if defined(LYNX_HAVE_NGX)
    if (device_ == nullptr)
    {
        return;
    }

    const NVSDK_NGX_Result result = callShutdown(device_);

    if (result != NVSDK_NGX_Result_Success)
    {
        spdlog::warn("ngx: NVSDK_NGX_D3D12_Shutdown1: {}", resultText(result));
    }
#endif

    device_ = nullptr;
    description_.clear();
}

bool NgxHost::initialized() const
{
    return device_ != nullptr;
}

const std::string& NgxHost::description() const
{
    return description_;
}

std::expected<NVSDK_NGX_Parameter*, std::string> NgxHost::allocateParameters() const
{
#if !defined(LYNX_HAVE_NGX)
    return std::unexpected("the build has no NVIDIA NGX SDK");
#else
    if (device_ == nullptr)
    {
        return std::unexpected("NGX isn't initialized");
    }

    NVSDK_NGX_Parameter* parameters = nullptr;
    const NVSDK_NGX_Result result = NVSDK_NGX_D3D12_AllocateParameters(&parameters);

    if (result != NVSDK_NGX_Result_Success || parameters == nullptr)
    {
        return std::unexpected("NVSDK_NGX_D3D12_AllocateParameters: " + resultText(result));
    }

    return parameters;
#endif
}

std::expected<NVSDK_NGX_Parameter*, std::string> NgxHost::capabilityParameters() const
{
#if !defined(LYNX_HAVE_NGX)
    return std::unexpected("the build has no NVIDIA NGX SDK");
#else
    if (device_ == nullptr)
    {
        return std::unexpected("NGX isn't initialized");
    }

    NVSDK_NGX_Parameter* parameters = nullptr;
    const NVSDK_NGX_Result result = NVSDK_NGX_D3D12_GetCapabilityParameters(&parameters);

    if (result != NVSDK_NGX_Result_Success || parameters == nullptr)
    {
        return std::unexpected("NVSDK_NGX_D3D12_GetCapabilityParameters: " + resultText(result));
    }

    return parameters;
#endif
}

void NgxHost::destroyParameters(NVSDK_NGX_Parameter* parameters)
{
#if defined(LYNX_HAVE_NGX)
    if (parameters != nullptr)
    {
        NVSDK_NGX_D3D12_DestroyParameters(parameters);
    }
#else
    (void)parameters;
#endif
}

} // namespace lynx::render
