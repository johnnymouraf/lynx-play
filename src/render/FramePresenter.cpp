#include "render/FramePresenter.h"

#include "core/Windows.h"
#include "render/ColorConversion.h"
#include "render/shaders/VideoFramePixelShader.h"
#include "render/shaders/VideoFrameVertexShader.h"

#include <array>
#include <climits>

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
constexpr UINT kViewsPerFrame = 2;
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

std::expected<ComPtr<ID3D12RootSignature>, std::string> createRootSignature(ID3D12Device& device)
{
    const D3D12_DESCRIPTOR_RANGE planes{
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        .NumDescriptors = kViewsPerFrame,
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

    const D3D12_STATIC_SAMPLER_DESC bilinear{
        .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .MaxLOD = D3D12_FLOAT32_MAX,
        .ShaderRegister = 0,
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
    };

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

std::expected<ComPtr<ID3D12PipelineState>, std::string> createPipeline(
    ID3D12Device& device, ID3D12RootSignature& rootSignature, DXGI_FORMAT targetFormat)
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    description.pRootSignature = &rootSignature;
    description.VS = {kVideoFrameVertexShader, sizeof(kVideoFrameVertexShader)};
    description.PS = {kVideoFramePixelShader, sizeof(kVideoFramePixelShader)};
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

} // namespace

std::expected<FramePresenter, std::string> FramePresenter::create(ID3D12Device& device,
                                                                  UINT bufferCount,
                                                                  DXGI_FORMAT targetFormat)
{
    FramePresenter presenter;
    presenter.device_ = &device;

    auto rootSignature = createRootSignature(device);

    if (!rootSignature)
    {
        return std::unexpected(rootSignature.error());
    }

    presenter.rootSignature_ = std::move(*rootSignature);
    auto pipeline = createPipeline(device, *presenter.rootSignature_.Get(), targetFormat);

    if (!pipeline)
    {
        return std::unexpected(pipeline.error());
    }

    presenter.pipeline_ = std::move(*pipeline);

    const D3D12_DESCRIPTOR_HEAP_DESC heapDescription{
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = bufferCount * kViewsPerFrame,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
    };

    if (const HRESULT result =
            device.CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&presenter.viewHeap_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateDescriptorHeap", result));
    }

    presenter.viewDescriptorSize_ =
        device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    return presenter;
}

void FramePresenter::record(ID3D12GraphicsCommandList& commandList, UINT bufferIndex,
                            const VideoFrame& frame, D3D12_CPU_DESCRIPTOR_HANDLE target,
                            SurfaceSize targetSize)
{
    const PixelRect area = fitPicture(targetSize, {.width = frame.width, .height = frame.height});

    if (area.width == 0 || area.height == 0)
    {
        return;
    }

    writeViews(bufferIndex, frame);
    const ConversionConstants constants = constantsFor(frame);

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
    commandList.SetGraphicsRootSignature(rootSignature_.Get());
    commandList.SetGraphicsRoot32BitConstants(kConstantsParameter, kConstantCount, &constants, 0);
    commandList.SetGraphicsRootDescriptorTable(kViewsParameter,
                                               gpuView(bufferIndex * kViewsPerFrame));
    commandList.SetPipelineState(pipeline_.Get());
    commandList.RSSetViewports(1, &viewport);
    commandList.RSSetScissorRects(1, &scissor);
    commandList.OMSetRenderTargets(1, &target, FALSE, nullptr);
    commandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList.DrawInstanced(3, 1, 0, 0);
}

void FramePresenter::writeViews(UINT bufferIndex, const VideoFrame& frame)
{
    const PlaneFormats formats = planeFormatsOf(frame.format);
    const std::array planeFormats{formats.luma, formats.chroma};

    for (UINT plane = 0; plane < kViewsPerFrame; ++plane)
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
                                          cpuView(bufferIndex * kViewsPerFrame + plane));
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

} // namespace lynx::render
