#include "render/OpticalFlow.h"

#include "core/Windows.h"
#include "render/AdapterSelection.h"
#include "render/GpuDevice.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

#include <nvOpticalFlowD3D12.h>
#include <spdlog/spdlog.h>

namespace lynx::render
{

namespace
{

using core::describeFailure;
using Microsoft::WRL::ComPtr;

constexpr DWORD kRegistrationTimeoutMilliseconds = 5000;
constexpr UINT kMinimumSlotCount = 2;

std::string_view statusName(NV_OF_STATUS status)
{
    switch (status)
    {
    case NV_OF_SUCCESS:
        return "NV_OF_SUCCESS";
    case NV_OF_ERR_OF_NOT_AVAILABLE:
        return "NV_OF_ERR_OF_NOT_AVAILABLE";
    case NV_OF_ERR_UNSUPPORTED_DEVICE:
        return "NV_OF_ERR_UNSUPPORTED_DEVICE";
    case NV_OF_ERR_DEVICE_DOES_NOT_EXIST:
        return "NV_OF_ERR_DEVICE_DOES_NOT_EXIST";
    case NV_OF_ERR_INVALID_PTR:
        return "NV_OF_ERR_INVALID_PTR";
    case NV_OF_ERR_INVALID_PARAM:
        return "NV_OF_ERR_INVALID_PARAM";
    case NV_OF_ERR_INVALID_CALL:
        return "NV_OF_ERR_INVALID_CALL";
    case NV_OF_ERR_INVALID_VERSION:
        return "NV_OF_ERR_INVALID_VERSION";
    case NV_OF_ERR_OUT_OF_MEMORY:
        return "NV_OF_ERR_OUT_OF_MEMORY";
    case NV_OF_ERR_NOT_INITIALIZED:
        return "NV_OF_ERR_NOT_INITIALIZED";
    case NV_OF_ERR_UNSUPPORTED_FEATURE:
        return "NV_OF_ERR_UNSUPPORTED_FEATURE";
    case NV_OF_ERR_GENERIC:
        return "NV_OF_ERR_GENERIC";
    }

    return "unknown status";
}

struct Api
{
    bool loaded = false;
    OpticalFlowUnavailable reason = OpticalFlowUnavailable::NoDriver;
    std::string detail;
    HMODULE module = nullptr;
    NV_OF_D3D12_API_FUNCTION_LIST functions{};
};

Api loadApi()
{
    Api api;

    api.module = LoadLibraryExW(L"nvofapi64.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (api.module == nullptr)
    {
        api.detail = "nvofapi64.dll isn't in System32 (an older NVIDIA driver)";
        return api;
    }

    HMODULE pinned = nullptr;

    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(api.module), &pinned) == FALSE)
    {
        api.detail = "nvofapi64.dll can't be pinned";
        return api;
    }

    const auto getVersion =
        reinterpret_cast<decltype(&NvOFGetMaxSupportedApiVersion)>(
            GetProcAddress(api.module, "NvOFGetMaxSupportedApiVersion"));

    if (getVersion == nullptr)
    {
        api.detail = "nvofapi64.dll has no NvOFGetMaxSupportedApiVersion";
        return api;
    }

    std::uint32_t version = 0;

    if (const NV_OF_STATUS status = getVersion(&version); status != NV_OF_SUCCESS)
    {
        api.detail = std::format("NvOFGetMaxSupportedApiVersion: {}", statusName(status));
        return api;
    }

    if (version < NV_OF_API_VERSION)
    {
        api.reason = OpticalFlowUnavailable::DriverTooOld;
        api.detail = std::format("NvOF API {}.{} is older than {}.{}", version >> 4, version & 0xF,
                                 NV_OF_API_MAJOR_VERSION, NV_OF_API_MINOR_VERSION);
        return api;
    }

    const auto createInstance =
        reinterpret_cast<decltype(&NvOFAPICreateInstanceD3D12)>(
            GetProcAddress(api.module, "NvOFAPICreateInstanceD3D12"));

    if (createInstance == nullptr)
    {
        api.detail = "nvofapi64.dll has no NvOFAPICreateInstanceD3D12";
        return api;
    }

    if (const NV_OF_STATUS status = createInstance(NV_OF_API_VERSION, &api.functions);
        status != NV_OF_SUCCESS)
    {
        api.detail = std::format("NvOFAPICreateInstanceD3D12: {}", statusName(status));
        return api;
    }

    api.loaded = true;
    api.detail = std::format("NvOF API {}.{}", version >> 4, version & 0xF);
    return api;
}

const Api& api()
{
    static const Api loaded = loadApi();
    return loaded;
}

std::expected<std::vector<DXGI_FORMAT>, std::string> surfaceFormats(
    const NV_OF_D3D12_API_FUNCTION_LIST& functions, NvOFHandle handle, NV_OF_BUFFER_USAGE usage)
{
    std::uint32_t count = 0;

    if (const NV_OF_STATUS status = functions.nvOFGetSurfaceFormatCountD3D12(
            handle, usage, NV_OF_MODE_OPTICALFLOW, &count);
        status != NV_OF_SUCCESS)
    {
        return std::unexpected(std::string(statusName(status)));
    }

    std::vector<DXGI_FORMAT> formats(count);

    if (count == 0)
    {
        return formats;
    }

    if (const NV_OF_STATUS status = functions.nvOFGetSurfaceFormatD3D12(
            handle, usage, NV_OF_MODE_OPTICALFLOW, formats.data());
        status != NV_OF_SUCCESS)
    {
        return std::unexpected(std::string(statusName(status)));
    }

    return formats;
}

bool supports(const std::vector<DXGI_FORMAT>& formats, DXGI_FORMAT format)
{
    return std::ranges::find(formats, format) != formats.end();
}

// nvOFGetCaps takes the number of uint32 elements, not bytes: a scalar is
// one, and a list follows the two-call protocol.
std::expected<std::uint32_t, std::string> cap(const NV_OF_D3D12_API_FUNCTION_LIST& functions,
                                             NvOFHandle handle, NV_OF_CAPS name)
{
    std::uint32_t value = 0;
    std::uint32_t count = 1;

    if (const NV_OF_STATUS status = functions.nvOFGetCaps(handle, name, &value, &count);
        status != NV_OF_SUCCESS)
    {
        return std::unexpected(std::string(statusName(status)));
    }

    return value;
}

bool supportsGrid(const NV_OF_D3D12_API_FUNCTION_LIST& functions, NvOFHandle handle,
                  std::uint32_t grid)
{
    std::uint32_t count = 0;

    if (functions.nvOFGetCaps(handle, NV_OF_CAPS_SUPPORTED_OUTPUT_GRID_SIZES, nullptr, &count) !=
            NV_OF_SUCCESS ||
        count == 0 || count > 64)
    {
        return false;
    }

    std::vector<std::uint32_t> sizes(count);
    std::uint32_t filled = count;

    if (functions.nvOFGetCaps(handle, NV_OF_CAPS_SUPPORTED_OUTPUT_GRID_SIZES, sizes.data(),
                               &filled) != NV_OF_SUCCESS)
    {
        return false;
    }

    return std::ranges::find(sizes, grid) != sizes.end();
}

std::string lastError(const NV_OF_D3D12_API_FUNCTION_LIST& functions, NvOFHandle handle)
{
    if (functions.nvOFGetLastError == nullptr)
    {
        return {};
    }

    char text[512]{};
    std::uint32_t size = sizeof(text);
    const NV_OF_STATUS status = functions.nvOFGetLastError(handle, text, &size);

    if (status != NV_OF_SUCCESS || text[0] == '\0')
    {
        return {};
    }

    return std::format(": {}", text);
}

D3D12_RESOURCE_DESC outputDescription(SurfaceSize extent, DXGI_FORMAT format)
{
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = extent.width;
    description.Height = extent.height;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = format;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    return description;
}

// The driver's release path can fault on some driver versions; the guard turns
// the access violation into a logged failure so a close can't take the process
// down. The function holds no objects that need unwinding, which SEH requires.
__declspec(noinline) bool callGuarded(const std::function<void()>& call, DWORD& fault)
{
    bool completed = false;

    __try
    {
        call();
        completed = true;
    }
    __except (fault = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
    {
        completed = false;
    }

    return completed;
}

} // namespace

struct OpticalFlow::Impl
{
    ~Impl()
    {
        releaseSession();
    }

    void releaseSession()
    {
        if (!configured && handle == nullptr && costs.empty() && flows.empty() && inputs.empty())
        {
            return;
        }

        spdlog::info("Optical flow: releasing the session on thread {} (created on thread {})",
                     GetCurrentThreadId(), createdThread);

        const auto step = [](std::string_view name, const std::function<void()>& call)
        {
            DWORD fault = 0;

            if (callGuarded(call, fault))
            {
                spdlog::info("Optical flow: {}", name);
                return;
            }

            spdlog::critical("Optical flow: {} faulted with 0x{:08X}", name, fault);
        };

        // A failed frame can leave the engine working; never release a
        // registered picture under it.
        if (lastOutputValue != 0 && fence != nullptr)
        {
            const std::uint64_t completed = fence->GetCompletedValue();

            if (completed < lastOutputValue && fenceEvent)
            {
                step(std::format("waiting for the last flow (fence {} of {})", completed,
                                 lastOutputValue),
                     [this]
                     {
                         if (SUCCEEDED(fence->SetEventOnCompletion(lastOutputValue,
                                                                   fenceEvent.get())))
                         {
                             WaitForSingleObject(fenceEvent.get(),
                                                 kRegistrationTimeoutMilliseconds);
                         }
                     });
            }
            else
            {
                spdlog::info("Optical flow: the last flow is complete (fence {} of {})", completed,
                             lastOutputValue);
            }

            lastOutputValue = 0;
        }

        const auto unregister = [this, &step](std::string_view name,
                                              std::vector<NvOFGPUBufferHandle>& handles)
        {
            for (NvOFGPUBufferHandle& buffer : handles)
            {
                if (buffer == nullptr)
                {
                    continue;
                }

                NV_OF_UNREGISTER_RESOURCE_PARAMS_D3D12 parameters{};
                parameters.hOFGpuBuffer = std::exchange(buffer, nullptr);
                step(std::format("unregistered a {} picture", name),
                     [this, &parameters] { functions.nvOFUnregisterResourceD3D12(&parameters); });
            }

            handles.clear();
        };

        const auto release = [&step](std::string_view name,
                                     std::vector<ComPtr<ID3D12Resource>>& resources)
        {
            for (std::size_t index = 0; index < resources.size(); ++index)
            {
                step(std::format("released {} picture {}", name, index),
                     [&resources, index] { resources[index].Reset(); });
            }

            resources.clear();
        };

        unregister("cost", costHandles);
        unregister("flow", flowHandles);
        unregister("input", inputHandles);
        release("cost", costs);
        release("flow", flows);
        release("input", inputs);

        if (handle != nullptr)
        {
            const NvOFHandle session = std::exchange(handle, nullptr);
            step("destroyed the NvOF session",
                 [this, session] { functions.nvOFDestroy(session); });
        }

        configured = false;
        spdlog::info("Optical flow: the session is released");
    }

    ComPtr<ID3D12Device> device;
    NV_OF_D3D12_API_FUNCTION_LIST functions{};
    DWORD createdThread = 0;
    NvOFHandle handle = nullptr;
    ComPtr<ID3D12Fence> fence;
    core::UniqueHandle fenceEvent;
    UINT slotCount = 0;
    SurfaceSize picture;
    SurfaceSize grid;
    std::vector<ComPtr<ID3D12Resource>> inputs;
    std::vector<ComPtr<ID3D12Resource>> flows;
    std::vector<ComPtr<ID3D12Resource>> costs;
    std::vector<NvOFGPUBufferHandle> inputHandles;
    std::vector<NvOFGPUBufferHandle> flowHandles;
    std::vector<NvOFGPUBufferHandle> costHandles;
    std::uint64_t nextFenceValue = 0;
    std::uint64_t lastOutputValue = 0;
    std::string description;
    bool configured = false;
};

FlowVector flowVectorPixels(std::int16_t flowX, std::int16_t flowY)
{
    return {
        .x = static_cast<float>(flowX) / kFlowFixedPointScale,
        .y = static_cast<float>(flowY) / kFlowFixedPointScale,
    };
}

SurfaceSize flowGridExtent(SurfaceSize picture, std::uint32_t grid)
{
    if (grid == 0)
    {
        return {};
    }

    return {
        .width = (picture.width + grid - 1) / grid,
        .height = (picture.height + grid - 1) / grid,
    };
}

std::optional<FlowVector> flowVectorAt(std::span<const std::int16_t> vectors,
                                        SurfaceSize gridExtent, SurfaceSize picture,
                                        std::uint32_t x, std::uint32_t y, std::uint32_t grid)
{
    if (grid == 0 || x >= picture.width || y >= picture.height)
    {
        return std::nullopt;
    }

    const std::uint32_t cellX = x / grid;
    const std::uint32_t cellY = y / grid;

    if (cellX >= gridExtent.width || cellY >= gridExtent.height)
    {
        return std::nullopt;
    }

    const std::size_t index = (static_cast<std::size_t>(cellY) * gridExtent.width + cellX) * 2;

    if (index + 1 >= vectors.size())
    {
        return std::nullopt;
    }

    return flowVectorPixels(vectors[index], vectors[index + 1]);
}

OpticalFlowAvailability OpticalFlow::probe()
{
    const Api& loaded = api();

    if (!loaded.loaded)
    {
        return {.available = false, .reason = loaded.reason, .detail = loaded.detail};
    }

    const auto stream = findStreamAdapter();

    if (!stream)
    {
        return {.available = false,
                .reason = OpticalFlowUnavailable::DriverFailed,
                .detail = stream.error()};
    }

    if (stream->info.vendorId != kNvidiaVendorId || stream->info.isSoftware)
    {
        return {.available = false,
                .reason = OpticalFlowUnavailable::NoNvidiaGpu,
                .detail = std::format("the stream renders on {}", stream->info.name)};
    }

    ComPtr<ID3D12Device> device;

    if (const HRESULT result =
            D3D12CreateDevice(stream->adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                              IID_PPV_ARGS(&device));
        FAILED(result))
    {
        return {.available = false,
                .reason = OpticalFlowUnavailable::DriverFailed,
                .detail = describeFailure("D3D12CreateDevice", result)};
    }

    NvOFHandle handle = nullptr;

    if (const NV_OF_STATUS status = loaded.functions.nvCreateOpticalFlowD3D12(device.Get(), &handle);
        status != NV_OF_SUCCESS || handle == nullptr)
    {
        return {.available = false,
                .reason = OpticalFlowUnavailable::DriverFailed,
                .detail = std::format("nvCreateOpticalFlowD3D12: {}", statusName(status))};
    }

    const auto destroy = [&loaded, &handle]
    {
        if (handle != nullptr)
        {
            loaded.functions.nvOFDestroy(handle);
            handle = nullptr;
        }
    };

    const auto inputFormats =
        surfaceFormats(loaded.functions, handle, NV_OF_BUFFER_USAGE_INPUT);
    const auto vectorFormats =
        surfaceFormats(loaded.functions, handle, NV_OF_BUFFER_USAGE_OUTPUT);
    const auto costFormats =
        surfaceFormats(loaded.functions, handle, NV_OF_BUFFER_USAGE_COST);

    if (!inputFormats || !vectorFormats || !costFormats)
    {
        const std::string detail = !inputFormats   ? inputFormats.error()
                                   : !vectorFormats ? vectorFormats.error()
                                                    : costFormats.error();
        destroy();
        return {.available = false,
                .reason = OpticalFlowUnavailable::FormatUnsupported,
                .detail = std::format("nvOFGetSurfaceFormatD3D12: {}", detail)};
    }

    if (!supports(*inputFormats, DXGI_FORMAT_B8G8R8A8_UNORM) ||
        !supports(*vectorFormats, DXGI_FORMAT_R16G16_SINT) ||
        !supports(*costFormats, DXGI_FORMAT_R8_UINT))
    {
        destroy();
        return {.available = false,
                .reason = OpticalFlowUnavailable::FormatUnsupported,
                .detail = "the driver lacks BGRA8 input with R16G16_SINT flow and R8_UINT cost"};
    }

    const auto width = cap(loaded.functions, handle, NV_OF_CAPS_WIDTH_MAX);
    const auto height = cap(loaded.functions, handle, NV_OF_CAPS_HEIGHT_MAX);

    if (!width || !height)
    {
        destroy();
        return {.available = false,
                .reason = OpticalFlowUnavailable::DriverFailed,
                .detail = std::format("nvOFGetCaps: {}", !width ? width.error() : height.error())};
    }

    if (!supportsGrid(loaded.functions, handle, kFlowGridSize))
    {
        destroy();
        return {.available = false,
                .reason = OpticalFlowUnavailable::FormatUnsupported,
                .detail = std::format("the driver doesn't take a {}x{} vector grid", kFlowGridSize,
                                      kFlowGridSize)};
    }

    destroy();
    return {
        .available = true,
        .reason = OpticalFlowUnavailable::None,
        .detail = std::format("{} on {} (up to {}x{})", loaded.detail, stream->info.name, *width,
                              *height),
    };
}

std::expected<OpticalFlow, std::string> OpticalFlow::create(ID3D12Device& device, UINT slotCount)
{
    const Api& loaded = api();

    if (!loaded.loaded)
    {
        return std::unexpected(loaded.detail);
    }

    if (slotCount < kMinimumSlotCount)
    {
        return std::unexpected("optical flow needs at least two picture slots");
    }

    auto impl = std::make_unique<Impl>();
    impl->device = &device;
    impl->functions = loaded.functions;
    impl->createdThread = GetCurrentThreadId();
    impl->slotCount = slotCount;

    if (const NV_OF_STATUS status =
            impl->functions.nvCreateOpticalFlowD3D12(&device, &impl->handle);
        status != NV_OF_SUCCESS || impl->handle == nullptr)
    {
        return std::unexpected(
            std::format("nvCreateOpticalFlowD3D12: {}", statusName(status)));
    }

    if (const HRESULT result =
            device.CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&impl->fence));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateFence (optical flow)", result));
    }

    impl->fenceEvent.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));

    if (!impl->fenceEvent)
    {
        return std::unexpected(
            describeFailure("CreateEventW (optical flow)", HRESULT_FROM_WIN32(GetLastError())));
    }

    return OpticalFlow(std::move(impl));
}

OpticalFlow::OpticalFlow(std::unique_ptr<Impl> impl) : impl_(std::move(impl))
{
}

OpticalFlow::OpticalFlow(OpticalFlow&&) noexcept = default;
OpticalFlow& OpticalFlow::operator=(OpticalFlow&&) noexcept = default;
OpticalFlow::~OpticalFlow() = default;

bool OpticalFlow::configuredFor(SurfaceSize picture) const
{
    return impl_ != nullptr && impl_->configured && impl_->picture == picture;
}

std::expected<void, std::string> OpticalFlow::configure(
    SurfaceSize picture, std::span<ID3D12Resource* const> inputs)
{
    if (impl_ == nullptr)
    {
        return std::unexpected("no optical flow session");
    }

    Impl& impl = *impl_;

    if (picture.width == 0 || picture.height == 0)
    {
        return std::unexpected("the picture has no size");
    }

    if (inputs.size() != impl.slotCount)
    {
        return std::unexpected(
            std::format("{} pictures for {} slots", inputs.size(), impl.slotCount));
    }

    bool sameInputs = impl.inputs.size() == inputs.size();

    for (std::size_t index = 0; sameInputs && index < inputs.size(); ++index)
    {
        sameInputs = impl.inputs.at(index).Get() == inputs[index];
    }

    if (impl.configured && impl.picture == picture && sameInputs)
    {
        return {};
    }

    // Unregistering and destroying the handle only happens between frames,
    // with the GPU idle.
    impl.releaseSession();

    if (const NV_OF_STATUS status =
            impl.functions.nvCreateOpticalFlowD3D12(impl.device.Get(), &impl.handle);
        status != NV_OF_SUCCESS || impl.handle == nullptr)
    {
        return std::unexpected(
            std::format("nvCreateOpticalFlowD3D12: {}", statusName(status)));
    }

    const auto width = cap(impl.functions, impl.handle, NV_OF_CAPS_WIDTH_MAX);
    const auto height = cap(impl.functions, impl.handle, NV_OF_CAPS_HEIGHT_MAX);

    if (!width || !height)
    {
        return std::unexpected(std::format("nvOFGetCaps: {}",
                                           !width ? width.error() : height.error()));
    }

    if (picture.width > *width || picture.height > *height)
    {
        return std::unexpected(std::format("{}x{} is above the engine's {}x{} limit",
                                           picture.width, picture.height, *width, *height));
    }

    const auto inputFormats =
        surfaceFormats(impl.functions, impl.handle, NV_OF_BUFFER_USAGE_INPUT);
    const auto vectorFormats =
        surfaceFormats(impl.functions, impl.handle, NV_OF_BUFFER_USAGE_OUTPUT);
    const auto costFormats =
        surfaceFormats(impl.functions, impl.handle, NV_OF_BUFFER_USAGE_COST);

    if (!inputFormats || !vectorFormats || !costFormats ||
        !supports(*inputFormats, DXGI_FORMAT_B8G8R8A8_UNORM) ||
        !supports(*vectorFormats, DXGI_FORMAT_R16G16_SINT) ||
        !supports(*costFormats, DXGI_FORMAT_R8_UINT))
    {
        return std::unexpected("the driver lacks BGRA8 input with R16G16_SINT flow and R8_UINT "
                               "cost");
    }

    NV_OF_INIT_PARAMS init{};
    init.width = picture.width;
    init.height = picture.height;
    init.outGridSize = static_cast<NV_OF_OUTPUT_VECTOR_GRID_SIZE>(kFlowGridSize);
    init.hintGridSize = NV_OF_HINT_VECTOR_GRID_SIZE_4;
    init.mode = NV_OF_MODE_OPTICALFLOW;
    init.perfLevel = NV_OF_PERF_LEVEL_MEDIUM;
    init.enableExternalHints = NV_OF_FALSE;
    init.enableOutputCost = NV_OF_TRUE;
    init.hPrivData = nullptr;
    init.disparityRange = NV_OF_STEREO_DISPARITY_RANGE_UNDEFINED;
    init.enableRoi = NV_OF_FALSE;
    init.predDirection = NV_OF_PRED_DIRECTION_FORWARD;
    init.enableGlobalFlow = NV_OF_FALSE;
    init.inputBufferFormat = NV_OF_BUFFER_FORMAT_ABGR8;

    if (const NV_OF_STATUS status = impl.functions.nvOFInit(impl.handle, &init);
        status != NV_OF_SUCCESS)
    {
        return std::unexpected(std::format("nvOFInit({}x{}): {}{}", picture.width, picture.height,
                                           statusName(status),
                                           lastError(impl.functions, impl.handle)));
    }

    impl.grid = flowGridExtent(picture, kFlowGridSize);
    const D3D12_HEAP_PROPERTIES heap{.Type = D3D12_HEAP_TYPE_DEFAULT};
    const D3D12_RESOURCE_DESC flowDescription = outputDescription(impl.grid, DXGI_FORMAT_R16G16_SINT);
    const D3D12_RESOURCE_DESC costDescription = outputDescription(impl.grid, DXGI_FORMAT_R8_UINT);

    for (UINT slot = 0; slot < impl.slotCount; ++slot)
    {
        ComPtr<ID3D12Resource> flow;
        ComPtr<ID3D12Resource> cost;

        if (const HRESULT result = impl.device->CreateCommittedResource(
                &heap, D3D12_HEAP_FLAG_NONE, &flowDescription, D3D12_RESOURCE_STATE_COMMON,
                nullptr, IID_PPV_ARGS(&flow));
            FAILED(result))
        {
            return std::unexpected(describeFailure("CreateCommittedResource (flow)", result));
        }

        if (const HRESULT result = impl.device->CreateCommittedResource(
                &heap, D3D12_HEAP_FLAG_NONE, &costDescription, D3D12_RESOURCE_STATE_COMMON,
                nullptr, IID_PPV_ARGS(&cost));
            FAILED(result))
        {
            return std::unexpected(describeFailure("CreateCommittedResource (flow cost)", result));
        }

        impl.flows.push_back(std::move(flow));
        impl.costs.push_back(std::move(cost));
    }

    for (UINT slot = 0; slot < impl.slotCount; ++slot)
    {
        impl.inputs.emplace_back(inputs[slot]);
    }

    ComPtr<ID3D12Fence> registrationFence;

    if (const HRESULT result =
            impl.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&registrationFence));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateFence (flow registration)", result));
    }

    std::uint64_t registered = 0;
    const auto registerResource = [&](ID3D12Resource* resource, NvOFGPUBufferHandle* handle)
        -> NV_OF_STATUS
    {
        NV_OF_REGISTER_RESOURCE_PARAMS_D3D12 parameters{};
        parameters.resource = resource;
        parameters.inputFencePoint = {registrationFence.Get(), registered};
        parameters.hOFGpuBuffer = handle;
        parameters.outputFencePoint = {registrationFence.Get(), registered + 1};

        return impl.functions.nvOFRegisterResourceD3D12(impl.handle, &parameters);
    };

    for (UINT slot = 0; slot < impl.slotCount; ++slot)
    {
        NvOFGPUBufferHandle handle = nullptr;
        const NV_OF_STATUS status = registerResource(impl.inputs.at(slot).Get(), &handle);

        if (status != NV_OF_SUCCESS)
        {
            return std::unexpected(std::format("nvOFRegisterResourceD3D12 (picture {}): {}{}",
                                               slot, statusName(status),
                                               lastError(impl.functions, impl.handle)));
        }

        impl.inputHandles.push_back(handle);
        ++registered;
    }

    for (UINT slot = 0; slot < impl.slotCount; ++slot)
    {
        NvOFGPUBufferHandle handle = nullptr;
        const NV_OF_STATUS status = registerResource(impl.flows.at(slot).Get(), &handle);

        if (status != NV_OF_SUCCESS)
        {
            return std::unexpected(std::format("nvOFRegisterResourceD3D12 (flow {}): {}{}", slot,
                                               statusName(status),
                                               lastError(impl.functions, impl.handle)));
        }

        impl.flowHandles.push_back(handle);
        ++registered;
    }

    for (UINT slot = 0; slot < impl.slotCount; ++slot)
    {
        NvOFGPUBufferHandle handle = nullptr;
        const NV_OF_STATUS status = registerResource(impl.costs.at(slot).Get(), &handle);

        if (status != NV_OF_SUCCESS)
        {
            return std::unexpected(std::format("nvOFRegisterResourceD3D12 (cost {}): {}{}", slot,
                                               statusName(status),
                                               lastError(impl.functions, impl.handle)));
        }

        impl.costHandles.push_back(handle);
        ++registered;
    }

    if (registrationFence->GetCompletedValue() < registered)
    {
        if (const HRESULT result =
                registrationFence->SetEventOnCompletion(registered, impl.fenceEvent.get());
            FAILED(result))
        {
            return std::unexpected(describeFailure("SetEventOnCompletion (flow registration)",
                                                   result));
        }

        if (WaitForSingleObject(impl.fenceEvent.get(), kRegistrationTimeoutMilliseconds) !=
            WAIT_OBJECT_0)
        {
            return std::unexpected("the driver didn't finish registering the flow resources");
        }
    }

    impl.picture = picture;
    impl.configured = true;
    impl.description = std::format("NvOF {}.{}, {}x{}, grid {}, medium", NV_OF_API_MAJOR_VERSION,
                                   NV_OF_API_MINOR_VERSION, picture.width, picture.height,
                                   kFlowGridSize);
    return {};
}

std::uint64_t OpticalFlow::nextInputValue()
{
    if (impl_ == nullptr)
    {
        return 0;
    }

    // Reserve the output value too: the engine signals one past the input.
    impl_->nextFenceValue += 2;
    return impl_->nextFenceValue - 1;
}

std::expected<std::uint64_t, std::string> OpticalFlow::execute(UINT slot,
                                                               std::uint64_t inputValue)
{
    if (impl_ == nullptr || !impl_->configured)
    {
        return std::unexpected("optical flow isn't configured");
    }

    Impl& impl = *impl_;

    if (slot >= impl.slotCount)
    {
        return std::unexpected(std::format("slot {} of {}", slot, impl.slotCount));
    }

    const UINT reference = (slot + impl.slotCount - 1) % impl.slotCount;
    const std::uint64_t outputValue = inputValue + 1;

    NV_OF_FENCE_POINT inputPoint{impl.fence.Get(), inputValue};
    NV_OF_EXECUTE_INPUT_PARAMS_D3D12 input{};
    input.inputFrame = impl.inputHandles.at(slot);
    input.referenceFrame = impl.inputHandles.at(reference);
    // Video has hard cuts; the temporal predictor smears across them.
    input.disableTemporalHints = NV_OF_TRUE;
    input.numFencePoints = 1;
    input.fencePoint = &inputPoint;

    NV_OF_FENCE_POINT outputPoint{impl.fence.Get(), outputValue};
    NV_OF_EXECUTE_OUTPUT_PARAMS_D3D12 output{};
    output.outputBuffer = impl.flowHandles.at(slot);
    output.outputCostBuffer = impl.costHandles.at(slot);
    output.fencePoint = &outputPoint;

    if (const NV_OF_STATUS status = impl.functions.nvOFExecuteD3D12(impl.handle, &input, &output);
        status != NV_OF_SUCCESS)
    {
        return std::unexpected(std::format("nvOFExecuteD3D12: {}{}", statusName(status),
                                           lastError(impl.functions, impl.handle)));
    }

    impl.lastOutputValue = outputValue;
    return outputValue;
}

ID3D12Fence* OpticalFlow::fence() const
{
    return impl_ != nullptr ? impl_->fence.Get() : nullptr;
}

std::uint64_t OpticalFlow::lastOutputValue() const
{
    return impl_ != nullptr ? impl_->lastOutputValue : 0;
}

ID3D12Resource* OpticalFlow::flow(UINT slot) const
{
    if (impl_ == nullptr || slot >= impl_->flows.size())
    {
        return nullptr;
    }

    return impl_->flows.at(slot).Get();
}

ID3D12Resource* OpticalFlow::cost(UINT slot) const
{
    if (impl_ == nullptr || slot >= impl_->costs.size())
    {
        return nullptr;
    }

    return impl_->costs.at(slot).Get();
}

SurfaceSize OpticalFlow::gridExtent() const
{
    return impl_ != nullptr ? impl_->grid : SurfaceSize{};
}

const std::string& OpticalFlow::description() const
{
    static const std::string empty;
    return impl_ != nullptr ? impl_->description : empty;
}

} // namespace lynx::render
