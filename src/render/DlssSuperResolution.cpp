#include "render/DlssSuperResolution.h"

#include "render/OpticalFlow.h"
#include "render/shaders/DlssMotionComputeShader.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include <windows.h>

#include <d3d12.h>

#ifdef LYNX_HAVE_NGX
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_helpers.h>
#endif

namespace lynx::render
{

namespace
{

using core::describeFailure;
using Microsoft::WRL::ComPtr;

constexpr std::uint32_t kMaxScale = 3;
constexpr float kConstantDepth = 0.9F;
constexpr UINT kMotionConstants = 5;
// One flow SRV and one motion UAV per back buffer.
constexpr UINT kDescriptorsPerSlot = 2;

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

D3D12_RESOURCE_DESC textureDescription(SurfaceSize size, DXGI_FORMAT format,
                                        D3D12_RESOURCE_FLAGS flags)
{
    return {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = size.width,
        .Height = size.height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = format,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = flags,
    };
}

std::expected<void, std::string> createMotionRootSignature(
    ID3D12Device& device, ComPtr<ID3D12RootSignature>& rootSignature)
{
    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0;

    D3D12_DESCRIPTOR_RANGE uavRange{};
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors = 1;
    uavRange.BaseShaderRegister = 0;

    std::array<D3D12_ROOT_PARAMETER, 3> parameters{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants = {
        .ShaderRegister = 0,
        .RegisterSpace = 0,
        .Num32BitValues = kMotionConstants,
    };
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = {
        .NumDescriptorRanges = 1,
        .pDescriptorRanges = &srvRange,
    };
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[2].DescriptorTable = {
        .NumDescriptorRanges = 1,
        .pDescriptorRanges = &uavRange,
    };
    parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    const D3D12_ROOT_SIGNATURE_DESC description{
        .NumParameters = static_cast<UINT>(parameters.size()),
        .pParameters = parameters.data(),
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE,
    };

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;

    if (const HRESULT result = D3D12SerializeRootSignature(
            &description, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
        FAILED(result))
    {
        return std::unexpected(describeFailure("D3D12SerializeRootSignature (DLSS)", result));
    }

    if (const HRESULT result =
            device.CreateRootSignature(0, serialized->GetBufferPointer(),
                                       serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateRootSignature (DLSS)", result));
    }

    return {};
}

} // namespace

bool dlssSuperResolutionEligible(SurfaceSize input, SurfaceSize output)
{
    if (input.width == 0 || input.height == 0 || output.width == 0 || output.height == 0)
    {
        return false;
    }

    if (output.width < input.width || output.height < input.height)
    {
        return false;
    }

    if (output.width == input.width && output.height == input.height)
    {
        return false;
    }

    const std::uint64_t widest = std::uint64_t{output.width} * kMaxScale;
    const std::uint64_t tallest = std::uint64_t{output.height} * kMaxScale;

    return input.width <= widest && input.height <= tallest;
}

struct DlssSuperResolution::Impl
{
    struct Slot
    {
        ComPtr<ID3D12Resource> output;
        ComPtr<ID3D12Resource> motion;
        // The output's current state: COMMON after creation, then
        // PIXEL_SHADER_RESOURCE once DLSS has written it.
        D3D12_RESOURCE_STATES outputState = D3D12_RESOURCE_STATE_COMMON;
    };

    ID3D12CommandQueue* queue = nullptr;
    ComPtr<ID3D12Device> device;
    UINT slotCount = 0;
    std::optional<DlssPicture> configured;
    std::string description;

    ComPtr<ID3D12DescriptorHeap> viewHeap;
    UINT viewDescriptorSize = 0;
    ComPtr<ID3D12DescriptorHeap> depthHeap;
    ComPtr<ID3D12RootSignature> motionRootSignature;
    ComPtr<ID3D12PipelineState> motionPipeline;
    ComPtr<ID3D12Resource> depth;

    ComPtr<ID3D12CommandAllocator> featureAllocator;
    ComPtr<ID3D12GraphicsCommandList> featureList;

    std::vector<Slot> slots;
    bool evaluatedOnce = false;

    D3D12_CPU_DESCRIPTOR_HANDLE viewHandle(UINT index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = viewHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(index) * viewDescriptorSize;

        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE gpuViewHandle(UINT index) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = viewHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<UINT64>(index) * viewDescriptorSize;

        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE depthView() const
    {
        return depthHeap->GetCPUDescriptorHandleForHeapStart();
    }

#ifdef LYNX_HAVE_NGX
    NVSDK_NGX_Handle* handle = nullptr;
    NVSDK_NGX_Parameter* parameters = nullptr;

    [[nodiscard]] std::expected<void, std::string> recordFeature(const DlssPicture& picture);

    void releaseFeature()
    {
        if (handle != nullptr)
        {
            NVSDK_NGX_D3D12_ReleaseFeature(handle);
            handle = nullptr;
        }
    }

    void releaseParameters()
    {
        if (parameters != nullptr)
        {
            NVSDK_NGX_DestroyParameters(parameters);
            parameters = nullptr;
        }
    }
#endif
};

namespace
{

#ifdef LYNX_HAVE_NGX

std::string ngxResult(NVSDK_NGX_Result result)
{
    return ngxResultText(result);
}

#endif

} // namespace

#ifdef LYNX_HAVE_NGX

std::expected<void, std::string> DlssSuperResolution::Impl::recordFeature(
    const DlssPicture& picture)
{
    unsigned int optimalWidth = 0;
    unsigned int optimalHeight = 0;
    unsigned int maxWidth = 0;
    unsigned int maxHeight = 0;
    unsigned int minWidth = 0;
    unsigned int minHeight = 0;
    float sharpness = 0.0F;

    const NVSDK_NGX_Result settings = NGX_DLSS_GET_OPTIMAL_SETTINGS(
        parameters, picture.output.width, picture.output.height,
        NVSDK_NGX_PerfQuality_Value_Balanced, &optimalWidth, &optimalHeight, &maxWidth,
        &maxHeight, &minWidth, &minHeight, &sharpness);

    if (NVSDK_NGX_FAILED(settings))
    {
        return std::unexpected(
            std::format("NGX_DLSS_GET_OPTIMAL_SETTINGS: {}", ngxResult(settings)));
    }

    const bool inRange = picture.input.width >= minWidth && picture.input.height >= minHeight &&
                         picture.input.width <= maxWidth && picture.input.height <= maxHeight;

    if (!inRange)
    {
        return std::unexpected(std::format(
            "{}x{} is outside DLSS's {}x{}..{}x{} render range for {}x{}", picture.input.width,
            picture.input.height, minWidth, minHeight, maxWidth, maxHeight, picture.output.width,
            picture.output.height));
    }

    NVSDK_NGX_DLSS_Create_Params create{};
    create.Feature.InWidth = picture.input.width;
    create.Feature.InHeight = picture.input.height;
    create.Feature.InTargetWidth = picture.output.width;
    create.Feature.InTargetHeight = picture.output.height;
    // The ratio comes from the sizes above; the quality label is cosmetic.
    create.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_Balanced;
    // Motion vectors are already at the render resolution and in pixels.
    create.InFeatureCreateFlags = static_cast<int>(NVSDK_NGX_DLSS_Feature_Flags_MVLowRes);
    create.InEnableOutputSubrects = false;

    const NVSDK_NGX_Result result =
        NGX_D3D12_CREATE_DLSS_EXT(featureList.Get(), 1, 1, &handle, parameters, &create);

    if (NVSDK_NGX_FAILED(result))
    {
        return std::unexpected(std::format("NGX_D3D12_CREATE_DLSS_EXT: {}", ngxResult(result)));
    }

    return {};
}

#endif

std::expected<DlssSuperResolution, std::string> DlssSuperResolution::create(
    NgxHost& host, ID3D12CommandQueue& queue, UINT slotCount)
{
#ifdef LYNX_HAVE_NGX
    if (slotCount == 0 || host.device() == nullptr)
    {
        return std::unexpected("DLSS needs a device and at least one back buffer");
    }

    auto impl = std::make_unique<Impl>();
    impl->queue = &queue;
    impl->device = host.device();
    impl->slotCount = slotCount;

    const D3D12_DESCRIPTOR_HEAP_DESC viewHeapDescription{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = slotCount * kDescriptorsPerSlot,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    };

    if (const HRESULT result = impl->device->CreateDescriptorHeap(
            &viewHeapDescription, IID_PPV_ARGS(&impl->viewHeap));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateDescriptorHeap (DLSS)", result));
    }

    impl->viewDescriptorSize =
        impl->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    const D3D12_DESCRIPTOR_HEAP_DESC depthHeapDescription{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        .NumDescriptors = 1,
    };

    if (const HRESULT result = impl->device->CreateDescriptorHeap(
            &depthHeapDescription, IID_PPV_ARGS(&impl->depthHeap));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateDescriptorHeap (DLSS depth)", result));
    }

    if (auto root = createMotionRootSignature(*impl->device.Get(), impl->motionRootSignature);
        !root)
    {
        return std::unexpected(root.error());
    }

    const D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDescription{
        .pRootSignature = impl->motionRootSignature.Get(),
        .CS = {kDlssMotionComputeShader, sizeof(kDlssMotionComputeShader)},
        .NodeMask = 0,
        .CachedPSO = {},
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
    };

    if (const HRESULT result = impl->device->CreateComputePipelineState(
            &pipelineDescription, IID_PPV_ARGS(&impl->motionPipeline));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateComputePipelineState (DLSS)", result));
    }

    if (const HRESULT result = impl->device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&impl->featureAllocator));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommandAllocator (DLSS)", result));
    }

    if (const HRESULT result = impl->device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, impl->featureAllocator.Get(), nullptr,
            IID_PPV_ARGS(&impl->featureList));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommandList (DLSS)", result));
    }

    impl->featureList->Close();

    const NVSDK_NGX_Result parameters =
        NVSDK_NGX_D3D12_GetCapabilityParameters(&impl->parameters);

    if (NVSDK_NGX_FAILED(parameters))
    {
        return std::unexpected(
            std::format("NVSDK_NGX_D3D12_GetCapabilityParameters: {}", ngxResult(parameters)));
    }

    impl->slots.resize(slotCount);
    return DlssSuperResolution(std::move(impl));
#else
    (void)host;
    (void)queue;
    (void)slotCount;
    return std::unexpected("the NGX SDK wasn't fetched (scripts/fetch-ngx.ps1)");
#endif
}

DlssSuperResolution::DlssSuperResolution(std::unique_ptr<Impl> impl) : impl_(std::move(impl))
{
}

DlssSuperResolution::DlssSuperResolution(DlssSuperResolution&&) noexcept = default;

DlssSuperResolution& DlssSuperResolution::operator=(DlssSuperResolution&&) noexcept = default;

DlssSuperResolution::~DlssSuperResolution()
{
#ifdef LYNX_HAVE_NGX
    if (impl_ != nullptr)
    {
        impl_->releaseFeature();
        impl_->releaseParameters();
    }
#endif
}

bool DlssSuperResolution::configuredFor(const DlssPicture& picture) const
{
    return impl_ != nullptr && impl_->configured.has_value() && *impl_->configured == picture;
}

std::expected<void, std::string> DlssSuperResolution::configure(const DlssPicture& picture)
{
    if (impl_ == nullptr)
    {
        return std::unexpected("no DLSS feature");
    }

    if (picture.input.width == 0 || picture.input.height == 0 || picture.output.width == 0 ||
        picture.output.height == 0)
    {
        return std::unexpected("the DLSS picture has no size");
    }

    if (configuredFor(picture))
    {
        return {};
    }

#ifdef LYNX_HAVE_NGX
    Impl& impl = *impl_;
    impl.releaseFeature();
    impl.configured.reset();
    impl.evaluatedOnce = false;

    const D3D12_HEAP_PROPERTIES heap{.Type = D3D12_HEAP_TYPE_DEFAULT};
    const D3D12_RESOURCE_DESC outputDescription =
        textureDescription(picture.output, DXGI_FORMAT_B8G8R8A8_UNORM,
                           D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    const D3D12_RESOURCE_DESC motionDescription =
        textureDescription(picture.input, DXGI_FORMAT_R16G16_FLOAT,
                           D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    D3D12_UNORDERED_ACCESS_VIEW_DESC motionView{};
    motionView.Format = DXGI_FORMAT_R16G16_FLOAT;
    motionView.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    motionView.Texture2D.MipSlice = 0;

    for (UINT slot = 0; slot < impl.slotCount; ++slot)
    {
        Impl::Slot& entry = impl.slots.at(slot);

        if (const HRESULT result = impl.device->CreateCommittedResource(
                &heap, D3D12_HEAP_FLAG_NONE, &outputDescription, D3D12_RESOURCE_STATE_COMMON,
                nullptr, IID_PPV_ARGS(&entry.output));
            FAILED(result))
        {
            return std::unexpected(
                describeFailure("CreateCommittedResource (DLSS output)", result));
        }

        if (const HRESULT result = impl.device->CreateCommittedResource(
                &heap, D3D12_HEAP_FLAG_NONE, &motionDescription, D3D12_RESOURCE_STATE_COMMON,
                nullptr, IID_PPV_ARGS(&entry.motion));
            FAILED(result))
        {
            return std::unexpected(
                describeFailure("CreateCommittedResource (DLSS motion vectors)", result));
        }

        entry.outputState = D3D12_RESOURCE_STATE_COMMON;
        impl.device->CreateUnorderedAccessView(entry.motion.Get(), nullptr, &motionView,
                                               impl.viewHandle(slot * kDescriptorsPerSlot + 1));
    }

    const D3D12_RESOURCE_DESC depthDescription =
        textureDescription(picture.input, DXGI_FORMAT_R32_FLOAT,
                           D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    if (const HRESULT result = impl.device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &depthDescription, D3D12_RESOURCE_STATE_DEPTH_WRITE,
            nullptr, IID_PPV_ARGS(&impl.depth));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateCommittedResource (DLSS depth)", result));
    }

    impl.device->CreateDepthStencilView(impl.depth.Get(), nullptr, impl.depthView());

    impl.featureAllocator->Reset();
    impl.featureList->Reset(impl.featureAllocator.Get(), nullptr);
    impl.featureList->ClearDepthStencilView(impl.depthView(), D3D12_CLEAR_FLAG_DEPTH,
                                             kConstantDepth, 0, 0, nullptr);

    const auto toShader = transition(impl.depth.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                     D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    impl.featureList->ResourceBarrier(1, &toShader);

    if (auto created = impl.recordFeature(picture); !created)
    {
        impl.featureList->Close();
        return created;
    }

    if (const HRESULT result = impl.featureList->Close(); FAILED(result))
    {
        return std::unexpected(
            describeFailure("ID3D12GraphicsCommandList::Close (DLSS depth)", result));
    }

    const std::array<ID3D12CommandList*, 1> lists{impl.featureList.Get()};
    impl.queue->ExecuteCommandLists(static_cast<UINT>(lists.size()), lists.data());

    impl.configured = picture;
    impl.description = std::format("DLSS {}x{} -> {}x{} with flow motion vectors",
                                   picture.input.width, picture.input.height,
                                   picture.output.width, picture.output.height);
    spdlog::info("DLSS SR: feature created for {}x{} -> {}x{} ({})", picture.input.width,
                 picture.input.height, picture.output.width, picture.output.height,
                 picture.toneMappedInput ? "tone-mapped SDR picture" : "the 8-bit picture");
    return {};
#else
    return std::unexpected("the NGX SDK wasn't fetched (scripts/fetch-ngx.ps1)");
#endif
}

ID3D12Resource* DlssSuperResolution::output(UINT slot) const
{
    if (impl_ == nullptr || slot >= impl_->slots.size())
    {
        return nullptr;
    }

    return impl_->slots.at(slot).output.Get();
}

std::expected<void, std::string> DlssSuperResolution::record(
    ID3D12GraphicsCommandList& commandList, UINT slot, ID3D12Resource& color,
    ID3D12Resource& flow, bool reset)
{
    if (impl_ == nullptr || !impl_->configured.has_value())
    {
        return std::unexpected("DLSS isn't configured");
    }

    if (slot >= impl_->slots.size())
    {
        return std::unexpected(std::format("DLSS slot {} of {}", slot, impl_->slots.size()));
    }

#ifdef LYNX_HAVE_NGX
    Impl& impl = *impl_;
    const DlssPicture& picture = *impl.configured;
    const SurfaceSize grid = flowGridExtent(picture.input);
    Impl::Slot& entry = impl.slots.at(slot);

    D3D12_SHADER_RESOURCE_VIEW_DESC flowView{};
    flowView.Format = DXGI_FORMAT_R16G16_SINT;
    flowView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    flowView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    flowView.Texture2D.MostDetailedMip = 0;
    flowView.Texture2D.MipLevels = 1;
    impl.device->CreateShaderResourceView(&flow, &flowView,
                                          impl.viewHandle(slot * kDescriptorsPerSlot));

    const auto motionToUav =
        transition(entry.motion.Get(), D3D12_RESOURCE_STATE_COMMON,
                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    const auto colorToCompute = transition(&color, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    // The engine leaves its grid in COMMON, where it reads it again next
    // frame; the densify pass takes it as a shader resource.
    const auto flowToShader = transition(&flow, D3D12_RESOURCE_STATE_COMMON,
                                         D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    const auto outputToUav = transition(entry.output.Get(), entry.outputState,
                                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    const std::array beforeBarriers{motionToUav, colorToCompute, flowToShader, outputToUav};
    commandList.ResourceBarrier(static_cast<UINT>(beforeBarriers.size()), beforeBarriers.data());

    const std::array<ID3D12DescriptorHeap*, 1> heaps{impl.viewHeap.Get()};
    commandList.SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    commandList.SetComputeRootSignature(impl.motionRootSignature.Get());

    struct MotionConstants
    {
        std::uint32_t pictureWidth;
        std::uint32_t pictureHeight;
        std::uint32_t gridWidth;
        std::uint32_t gridHeight;
        std::uint32_t gridSize;
    };

    const MotionConstants constants{
        .pictureWidth = picture.input.width,
        .pictureHeight = picture.input.height,
        .gridWidth = grid.width,
        .gridHeight = grid.height,
        .gridSize = kFlowGridSize,
    };

    commandList.SetComputeRoot32BitConstants(0, kMotionConstants, &constants, 0);
    commandList.SetComputeRootDescriptorTable(
        1, impl.gpuViewHandle(slot * kDescriptorsPerSlot));
    commandList.SetComputeRootDescriptorTable(
        2, impl.gpuViewHandle(slot * kDescriptorsPerSlot + 1));
    commandList.SetPipelineState(impl.motionPipeline.Get());
    commandList.Dispatch((picture.input.width + 7) / 8, (picture.input.height + 7) / 8, 1);

    const auto motionToShader =
        transition(entry.motion.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList.ResourceBarrier(1, &motionToShader);

    NVSDK_NGX_D3D12_DLSS_Eval_Params params{};
    params.Feature.pInColor = &color;
    params.Feature.pInOutput = entry.output.Get();
    params.Feature.InSharpness = 0.0F;
    params.pInDepth = impl.depth.Get();
    params.pInMotionVectors = &flow;
    params.InJitterOffsetX = 0.0F;
    params.InJitterOffsetY = 0.0F;
    params.InRenderSubrectDimensions = {.Width = picture.input.width,
                                         .Height = picture.input.height};
    params.InReset = reset ? 1 : 0;
    params.InMVScaleX = 1.0F;
    params.InMVScaleY = 1.0F;

    const NVSDK_NGX_Result evaluated =
        NGX_D3D12_EVALUATE_DLSS_EXT(&commandList, impl.handle, impl.parameters, &params);

    if (NVSDK_NGX_FAILED(evaluated))
    {
        return std::unexpected(
            std::format("NGX_D3D12_EVALUATE_DLSS_EXT: {}", ngxResult(evaluated)));
    }

    const auto outputToShader =
        transition(entry.output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const auto motionToCommon =
        transition(entry.motion.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_COMMON);
    const auto colorToShader = transition(&color, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    const auto flowToCommon = transition(&flow, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                          D3D12_RESOURCE_STATE_COMMON);
    const std::array afterBarriers{outputToShader, motionToCommon, colorToShader, flowToCommon};
    commandList.ResourceBarrier(static_cast<UINT>(afterBarriers.size()), afterBarriers.data());

    entry.outputState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    if (!impl.evaluatedOnce)
    {
        impl.evaluatedOnce = true;
        spdlog::info("DLSS SR: evaluating with the optical flow's motion vectors{}",
                     reset ? " (history reset)" : "");
    }

    return {};
#else
    (void)commandList;
    (void)slot;
    (void)color;
    (void)flow;
    (void)reset;
    return std::unexpected("the NGX SDK wasn't fetched (scripts/fetch-ngx.ps1)");
#endif
}

const std::string& DlssSuperResolution::description() const
{
    static const std::string empty;

    return impl_ != nullptr ? impl_->description : empty;
}

} // namespace lynx::render
