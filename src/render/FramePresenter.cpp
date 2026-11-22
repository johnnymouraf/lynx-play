#include "render/FramePresenter.h"

#include "core/Windows.h"
#include "render/ColorConversion.h"
#include "render/shaders/ScalePixelShader.h"
#include "render/shaders/ScaleVertexShader.h"
#include "render/shaders/VideoFramePixelShader.h"
#include "render/shaders/VideoFrameVertexShader.h"

#include <array>
#include <climits>
#include <cstdint>
#include <span>

namespace lynx::render
{

namespace
{

using core::describeFailure;
using Microsoft::WRL::ComPtr;

// The pixel shader's cbuffer, passed as root constants.
struct ConversionConstants
{
    std::array<std::array<float, 4>, 3> rows{};
    std::array<float, 2> visibleExtent{};
    std::array<float, 2> padding{};
};

constexpr UINT kConstantCount = sizeof(ConversionConstants) / sizeof(float);
// Luma and chroma views, then the intermediate picture.
constexpr UINT kViewsPerBuffer = 3;
constexpr UINT kConstantsParameter = 0;
constexpr UINT kViewsParameter = 1;

struct PlaneFormats
{
    DXGI_FORMAT luma = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT chroma = DXGI_FORMAT_UNKNOWN;
};

PlaneFormats planeFormatsOf(PixelFormat format)
{
    if (format == PixelFormat::P010)
    {
        return {.luma = DXGI_FORMAT_R16_UNORM, .chroma = DXGI_FORMAT_R16G16_UNORM};
    }

    return {.luma = DXGI_FORMAT_R8_UNORM, .chroma = DXGI_FORMAT_R8G8_UNORM};
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

D3D12_STATIC_SAMPLER_DESC bilinearSampler()
{
    return {
        .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .MaxLOD = D3D12_FLOAT32_MAX,
        .ShaderRegister = 0,
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
    };
}

std::expected<ComPtr<ID3D12RootSignature>, std::string> createConversionRootSignature(
    ID3D12Device& device)
{
    const D3D12_DESCRIPTOR_RANGE planes{
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        .NumDescriptors = kViewsPerBuffer,
        .BaseShaderRegister = 0,
        .RegisterSpace = 0,
        .OffsetInDescriptorsFromTableStart = 0,
    };

    std::array<D3D12_ROOT_PARAMETER, 2> parameters{};
    parameters[kConstantsParameter].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[kConstantsParameter].Constants = {
        .ShaderRegister = 0,
        .RegisterSpace = 0,
        .Num32BitValues = kConstantCount,
    };
    parameters[kConstantsParameter].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[kViewsParameter].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[kViewsParameter].DescriptorTable = {
        .NumDescriptorRanges = 1,
        .pDescriptorRanges = &planes,
    };
    parameters[kViewsParameter].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    const D3D12_STATIC_SAMPLER_DESC bilinear = bilinearSampler();
    const D3D12_ROOT_SIGNATURE_DESC description{
        .NumParameters = static_cast<UINT>(parameters.size()),
        .pParameters = parameters.data(),
        .NumStaticSamplers = 1,
        .pStaticSamplers = &bilinear,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS,
    };

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;

    if (const HRESULT result = D3D12SerializeRootSignature(
            &description, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
        FAILED(result))
    {
        return std::unexpected(describeFailure("D3D12SerializeRootSignature", result));
    }

    ComPtr<ID3D12RootSignature> rootSignature;

    if (const HRESULT result =
            device.CreateRootSignature(0, serialized->GetBufferPointer(),
                                       serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateRootSignature", result));
    }

    return rootSignature;
}

std::expected<ComPtr<ID3D12RootSignature>, std::string> createScalingRootSignature(
    ID3D12Device& device)
{
    const D3D12_DESCRIPTOR_RANGE picture{
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        .NumDescriptors = 1,
        .BaseShaderRegister = 0,
        .RegisterSpace = 0,
        .OffsetInDescriptorsFromTableStart = 0,
    };

    D3D12_ROOT_PARAMETER parameter{};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameter.DescriptorTable = {
        .NumDescriptorRanges = 1,
        .pDescriptorRanges = &picture,
    };
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    const D3D12_STATIC_SAMPLER_DESC bilinear = bilinearSampler();
    const D3D12_ROOT_SIGNATURE_DESC description{
        .NumParameters = 1,
        .pParameters = &parameter,
        .NumStaticSamplers = 1,
        .pStaticSamplers = &bilinear,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                 D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS,
    };

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;

    if (const HRESULT result = D3D12SerializeRootSignature(
            &description, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
        FAILED(result))
    {
        return std::unexpected(describeFailure("D3D12SerializeRootSignature", result));
    }

    ComPtr<ID3D12RootSignature> rootSignature;

    if (const HRESULT result =
            device.CreateRootSignature(0, serialized->GetBufferPointer(),
                                       serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateRootSignature", result));
    }

    return rootSignature;
}

std::expected<ComPtr<ID3D12PipelineState>, std::string> createPipeline(
    ID3D12Device& device, ID3D12RootSignature& rootSignature, DXGI_FORMAT targetFormat,
    std::span<const std::uint8_t> vertexShader, std::span<const std::uint8_t> pixelShader)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    description.pRootSignature = &rootSignature;
    description.VS = {vertexShader.data(), vertexShader.size()};
    description.PS = {pixelShader.data(), pixelShader.size()};
    description.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    description.SampleMask = UINT_MAX;
    description.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    description.RasterizerState.DepthClipEnable = TRUE;
    description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    description.NumRenderTargets = 1;
    description.RTVFormats[0] = targetFormat;
    description.SampleDesc = {.Count = 1, .Quality = 0};

    ComPtr<ID3D12PipelineState> pipeline;

    if (const HRESULT result =
            device.CreateGraphicsPipelineState(&description, IID_PPV_ARGS(&pipeline));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateGraphicsPipelineState", result));
    }

    return pipeline;
}

ConversionConstants constantsFor(const VideoFrame& frame)
{
    const D3D12_RESOURCE_DESC texture = frame.texture->GetDesc();

    return {
        .rows = yuvToRgb(frame.matrix, frame.range, frame.format).rows,
        .visibleExtent = {static_cast<float>(frame.width) / static_cast<float>(texture.Width),
                          static_cast<float>(frame.height) / static_cast<float>(texture.Height)},
        .padding = {},
    };
}

D3D12_RESOURCE_DESC intermediateDescription(SurfaceSize picture)
{
    return {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = picture.width,
        .Height = picture.height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
    };
}

} // namespace

std::expected<FramePresenter, std::string> FramePresenter::create(ID3D12Device& device,
                                                                  UINT bufferCount,
                                                                  DXGI_FORMAT targetFormat)
{
    FramePresenter presenter;
    presenter.device_ = &device;
    presenter.bufferCount_ = bufferCount;

    auto conversionRoot = createConversionRootSignature(device);

    if (!conversionRoot)
    {
        return std::unexpected(conversionRoot.error());
    }

    presenter.conversionRootSignature_ = std::move(*conversionRoot);

    auto scalingRoot = createScalingRootSignature(device);

    if (!scalingRoot)
    {
        return std::unexpected(scalingRoot.error());
    }

    presenter.scalingRootSignature_ = std::move(*scalingRoot);

    auto conversionPipeline =
        createPipeline(device, *presenter.conversionRootSignature_.Get(), targetFormat,
                       {kVideoFrameVertexShader, sizeof(kVideoFrameVertexShader)},
                       {kVideoFramePixelShader, sizeof(kVideoFramePixelShader)});

    if (!conversionPipeline)
    {
        return std::unexpected(conversionPipeline.error());
    }

    presenter.conversionPipeline_ = std::move(*conversionPipeline);

    auto scalingPipeline =
        createPipeline(device, *presenter.scalingRootSignature_.Get(), targetFormat,
                       {kScaleVertexShader, sizeof(kScaleVertexShader)},
                       {kScalePixelShader, sizeof(kScalePixelShader)});

    if (!scalingPipeline)
    {
        return std::unexpected(scalingPipeline.error());
    }

    presenter.scalingPipeline_ = std::move(*scalingPipeline);

    const D3D12_DESCRIPTOR_HEAP_DESC viewHeapDescription{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = bufferCount * kViewsPerBuffer,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    };

    if (const HRESULT result =
            device.CreateDescriptorHeap(&viewHeapDescription, IID_PPV_ARGS(&presenter.viewHeap_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateDescriptorHeap", result));
    }

    presenter.viewDescriptorSize_ =
        device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    const D3D12_DESCRIPTOR_HEAP_DESC targetHeapDescription{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = bufferCount,
    };

    if (const HRESULT result = device.CreateDescriptorHeap(&targetHeapDescription,
                                                           IID_PPV_ARGS(
                                                               &presenter.intermediateTargetHeap_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateDescriptorHeap (intermediates)", result));
    }

    presenter.intermediateTargetDescriptorSize_ =
        device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    return presenter;
}

bool FramePresenter::needsPicture(SurfaceSize picture) const
{
    return picture != pictureSize_;
}

std::expected<void, std::string> FramePresenter::resizePicture(SurfaceSize picture)
{
    if (picture.width == 0 || picture.height == 0)
    {
        return {};
    }

    const D3D12_RESOURCE_DESC description = intermediateDescription(picture);
    const D3D12_HEAP_PROPERTIES defaultHeap{.Type = D3D12_HEAP_TYPE_DEFAULT};

    intermediates_.clear();
    intermediates_.resize(bufferCount_);

    for (UINT index = 0; index < static_cast<UINT>(bufferCount); ++index)
    {
        ComPtr<ID3D12Resource>& texture = intermediates_.at(index);

        if (const HRESULT result = device_->CreateCommittedResource(
                &defaultHeap, D3D12_HEAP_FLAG_NONE, &description,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&texture));
            FAILED(result))
        {
            return std::unexpected(describeFailure("CreateCommittedResource (picture)", result));
        }

        device_->CreateRenderTargetView(texture.Get(), nullptr, cpuRenderTarget(index));
        device_->CreateShaderResourceView(
            texture.Get(), nullptr,
            cpuView(index * kViewsPerBuffer + kViewsPerBuffer - 1));
    }

    pictureSize_ = picture;
    return {};
}

void FramePresenter::recordConversion(ID3D12GraphicsCommandList& commandList, UINT bufferIndex,
                                      const VideoFrame& frame)
{
    if (bufferIndex >= intermediates_.size())
    {
        return;
    }

    ID3D12Resource* const intermediate = intermediates_.at(bufferIndex).Get();

    const auto toTarget =
        transition(intermediate, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                   D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList.ResourceBarrier(1, &toTarget);

    writeInputViews(bufferIndex, frame);
    const ConversionConstants constants = constantsFor(frame);

    const D3D12_VIEWPORT viewport{
        .TopLeftX = 0.0F,
        .TopLeftY = 0.0F,
        .Width = static_cast<float>(pictureSize_.width),
        .Height = static_cast<float>(pictureSize_.height),
        .MinDepth = 0.0F,
        .MaxDepth = 1.0F,
    };
    const D3D12_RECT scissor{
        .left = 0,
        .top = 0,
        .right = static_cast<LONG>(pictureSize_.width),
        .bottom = static_cast<LONG>(pictureSize_.height),
    };

    const std::array<ID3D12DescriptorHeap*, 1> heaps{viewHeap_.Get()};
    commandList.SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    commandList.SetGraphicsRootSignature(conversionRootSignature_.Get());
    commandList.SetGraphicsRoot32BitConstants(kConstantsParameter, kConstantCount, &constants, 0);
    commandList.SetGraphicsRootDescriptorTable(kViewsParameter,
                                               gpuView(bufferIndex * kViewsPerBuffer));
    commandList.SetPipelineState(conversionPipeline_.Get());
    commandList.RSSetViewports(1, &viewport);
    commandList.RSSetScissorRects(1, &scissor);

    const D3D12_CPU_DESCRIPTOR_HANDLE target = cpuRenderTarget(bufferIndex);
    commandList.OMSetRenderTargets(1, &target, FALSE, nullptr);
    commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList.DrawInstanced(3, 1, 0, 0);

    const auto toShader = transition(intermediate, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                     D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList.ResourceBarrier(1, &toShader);
}

void FramePresenter::recordScaling(ID3D12GraphicsCommandList& commandList, UINT bufferIndex,
                                   D3D12_CPU_DESCRIPTOR_HANDLE target, SurfaceSize targetSize)
{
    const PixelRect area = fitPicture(targetSize, pictureSize_);

    if (area.width == 0 || area.height == 0)
    {
        return;
    }

    const D3D12_VIEWPORT viewport{
        .TopLeftX = static_cast<float>(area.left),
        .TopLeftY = static_cast<float>(area.top),
        .Width = static_cast<float>(area.width),
        .Height = static_cast<float>(area.height),
        .MinDepth = 0.0F,
        .MaxDepth = 1.0F,
    };
    const D3D12_RECT scissor{
        .left = static_cast<LONG>(area.left),
        .top = static_cast<LONG>(area.top),
        .right = static_cast<LONG>(area.left + area.width),
        .bottom = static_cast<LONG>(area.top + area.height),
    };

    const std::array<ID3D12DescriptorHeap*, 1> heaps{viewHeap_.Get()};
    commandList.SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    commandList.SetGraphicsRootSignature(scalingRootSignature_.Get());
    commandList.SetGraphicsRootDescriptorTable(
        0, gpuView(bufferIndex * kViewsPerBuffer + kViewsPerBuffer - 1));
    commandList.SetPipelineState(scalingPipeline_.Get());
    commandList.RSSetViewports(1, &viewport);
    commandList.RSSetScissorRects(1, &scissor);
    commandList.OMSetRenderTargets(1, &target, FALSE, nullptr);
    commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList.DrawInstanced(3, 1, 0, 0);
}

void FramePresenter::writeInputViews(UINT bufferIndex, const VideoFrame& frame)
{
    const PlaneFormats formats = planeFormatsOf(frame.format);
    const std::array planeFormats{formats.luma, formats.chroma};

    for (UINT plane = 0; plane < 2; ++plane)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC view{
            .Format = planeFormats.at(plane),
            .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY,
            .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        };
        view.Texture2DArray = {
            .MostDetailedMip = 0,
            .MipLevels = 1,
            .FirstArraySlice = frame.arraySlice,
            .ArraySize = 1,
            .PlaneSlice = plane,
            .ResourceMinLODClamp = 0.0F,
        };

        device_->CreateShaderResourceView(frame.texture.Get(), &view,
                                          cpuView(bufferIndex * kViewsPerBuffer + plane));
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE FramePresenter::cpuView(UINT index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = viewHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * viewDescriptorSize_;

    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE FramePresenter::gpuView(UINT index) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = viewHeap_->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(index) * viewDescriptorSize_;

    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE FramePresenter::cpuRenderTarget(UINT index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        intermediateTargetHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * intermediateTargetDescriptorSize_;

    return handle;
}

} // namespace lynx::render
