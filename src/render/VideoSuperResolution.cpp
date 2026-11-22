#include "render/VideoSuperResolution.h"

#include <format>
#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>

namespace lynx::render
{

namespace
{

using core::describeFailure;
using Microsoft::WRL::ComPtr;

// NVIDIA's private video-processor extension: version 1, method 2 is super
// resolution (method 3 is TrueHDR). There is no quality field; the NVIDIA
// App's global RTX Video Enhancement setting owns the model and its quality.
constexpr GUID kNvidiaPpeInterface{
    0xd43ce1b3, 0x1f4b, 0x48ac, {0xba, 0xee, 0xc3, 0xc2, 0x53, 0x75, 0xe6, 0xf7}};

struct NvidiaPpeRequest
{
    UINT version = 1;
    UINT method = 2;
    UINT enable = 1;
};

constexpr DXGI_FORMAT kOutputFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_COLOR_SPACE_TYPE kOutputColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
constexpr SurfaceSize kProbeInput{.width = 1280, .height = 720};
constexpr SurfaceSize kProbeOutput{.width = 1920, .height = 1080};

struct VideoDevice
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Device5> device5;
    ComPtr<ID3D11DeviceContext4> context4;
    ComPtr<ID3D11VideoDevice> videoDevice;
    ComPtr<ID3D11VideoContext1> videoContext;
};

struct VideoProcessor
{
    ComPtr<ID3D11VideoProcessorEnumerator> enumerator;
    ComPtr<ID3D11VideoProcessor> processor;
    std::string description;
};

struct VsrFailure
{
    VsrUnavailable reason = VsrUnavailable::ProcessorFailed;
    std::string detail;
};

DXGI_FORMAT inputFormatOf(PixelFormat format)
{
    return format == PixelFormat::P010 ? DXGI_FORMAT_P010 : DXGI_FORMAT_NV12;
}

DXGI_COLOR_SPACE_TYPE inputColorSpaceOf(ColorMatrix matrix, ColorRange range)
{
    switch (matrix)
    {
    case ColorMatrix::Bt601:
        return range == ColorRange::Full ? DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601
                                         : DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601;
    case ColorMatrix::Bt709:
        return range == ColorRange::Full ? DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709
                                         : DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709;
    case ColorMatrix::Bt2020:
        return range == ColorRange::Full ? DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020
                                         : DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020;
    }

    return DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709;
}

std::string formatName(PixelFormat format)
{
    return format == PixelFormat::P010 ? "P010" : "NV12";
}

std::string colorSpaceName(ColorMatrix matrix, ColorRange range)
{
    std::string_view matrixName = "BT.709";

    switch (matrix)
    {
    case ColorMatrix::Bt601:
        matrixName = "BT.601";
        break;
    case ColorMatrix::Bt709:
        matrixName = "BT.709";
        break;
    case ColorMatrix::Bt2020:
        matrixName = "BT.2020";
        break;
    }

    const std::string_view rangeName = range == ColorRange::Full ? "full" : "studio";

    return std::format("{} {}", matrixName, rangeName);
}

std::expected<VideoDevice, VsrFailure> openVideoDevice(IDXGIAdapter4& adapter)
{
    VideoDevice video;
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    constexpr D3D_FEATURE_LEVEL kLevels[]{D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    constexpr UINT kLevelCount = 2;
    constexpr UINT kFlags =
        static_cast<UINT>(D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT);

    if (const HRESULT result = D3D11CreateDevice(
            &adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, kFlags, kLevels, kLevelCount,
            D3D11_SDK_VERSION, video.device.GetAddressOf(), &featureLevel,
            video.context.GetAddressOf());
        FAILED(result))
    {
        return std::unexpected(
            VsrFailure{VsrUnavailable::NoD3D11, describeFailure("D3D11CreateDevice", result)});
    }

    if (const HRESULT result = video.device.As(&video.device5); FAILED(result))
    {
        return std::unexpected(
            VsrFailure{VsrUnavailable::NoD3D11, describeFailure("ID3D11Device5", result)});
    }

    if (const HRESULT result = video.context.As(&video.context4); FAILED(result))
    {
        return std::unexpected(
            VsrFailure{VsrUnavailable::NoD3D11, describeFailure("ID3D11DeviceContext4", result)});
    }

    if (const HRESULT result = video.device.As(&video.videoDevice); FAILED(result))
    {
        return std::unexpected(
            VsrFailure{VsrUnavailable::NoD3D11, describeFailure("ID3D11VideoDevice", result)});
    }

    if (const HRESULT result = video.context.As(&video.videoContext); FAILED(result))
    {
        return std::unexpected(
            VsrFailure{VsrUnavailable::NoD3D11, describeFailure("ID3D11VideoContext1", result)});
    }

    return video;
}

std::expected<VideoProcessor, VsrFailure> createProcessor(ID3D11VideoDevice& videoDevice,
                                                          ID3D11VideoContext1& videoContext,
                                                          const VsrPicture& picture)
{
    const D3D11_VIDEO_PROCESSOR_CONTENT_DESC content{
        .InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE,
        .InputFrameRate = {.Numerator = 60, .Denominator = 1},
        .InputWidth = picture.input.width,
        .InputHeight = picture.input.height,
        .OutputFrameRate = {.Numerator = 60, .Denominator = 1},
        .OutputWidth = picture.output.width,
        .OutputHeight = picture.output.height,
        .Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL,
    };

    VideoProcessor processor;

    if (const HRESULT result = videoDevice.CreateVideoProcessorEnumerator(
            &content, processor.enumerator.GetAddressOf());
        FAILED(result))
    {
        return std::unexpected(
            VsrFailure{VsrUnavailable::ProcessorFailed,
                       describeFailure("CreateVideoProcessorEnumerator", result)});
    }

    ComPtr<ID3D11VideoProcessorEnumerator1> enumerator1;

    if (const HRESULT result = processor.enumerator.As(&enumerator1); FAILED(result))
    {
        return std::unexpected(
            VsrFailure{VsrUnavailable::FormatUnsupported,
                       describeFailure("ID3D11VideoProcessorEnumerator1", result)});
    }

    const DXGI_FORMAT inputFormat = inputFormatOf(picture.format);
    const DXGI_COLOR_SPACE_TYPE inputColorSpace =
        inputColorSpaceOf(picture.matrix, picture.range);
    BOOL supported = FALSE;
    const HRESULT conversion = enumerator1->CheckVideoProcessorFormatConversion(
        inputFormat, inputColorSpace, kOutputFormat, kOutputColorSpace, &supported);

    if (SUCCEEDED(conversion) && supported == FALSE)
    {
        return std::unexpected(VsrFailure{
            VsrUnavailable::FormatUnsupported,
            std::format("the driver doesn't convert {} {} to R8G8B8A8", formatName(picture.format),
                        colorSpaceName(picture.matrix, picture.range)),
        });
    }

    if (FAILED(conversion))
    {
        return std::unexpected(VsrFailure{VsrUnavailable::FormatUnsupported,
                                          describeFailure("CheckVideoProcessorFormatConversion",
                                                          conversion)});
    }

    if (const HRESULT result = videoDevice.CreateVideoProcessor(
            processor.enumerator.Get(), 0, processor.processor.GetAddressOf());
        FAILED(result))
    {
        return std::unexpected(VsrFailure{VsrUnavailable::ProcessorFailed,
                                          describeFailure("CreateVideoProcessor", result)});
    }

    ID3D11VideoProcessor* const videoProcessor = processor.processor.Get();
    videoContext.VideoProcessorSetStreamAutoProcessingMode(videoProcessor, 0, FALSE);
    videoContext.VideoProcessorSetStreamFrameFormat(videoProcessor, 0,
                                                    D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    videoContext.VideoProcessorSetStreamColorSpace1(videoProcessor, 0, inputColorSpace);
    videoContext.VideoProcessorSetOutputColorSpace1(videoProcessor, kOutputColorSpace);

    const RECT source{
        .left = 0,
        .top = 0,
        .right = static_cast<LONG>(picture.input.width),
        .bottom = static_cast<LONG>(picture.input.height),
    };
    videoContext.VideoProcessorSetStreamSourceRect(videoProcessor, 0, TRUE, &source);

    const RECT destination{
        .left = 0,
        .top = 0,
        .right = static_cast<LONG>(picture.output.width),
        .bottom = static_cast<LONG>(picture.output.height),
    };
    videoContext.VideoProcessorSetStreamDestRect(videoProcessor, 0, TRUE, &destination);
    videoContext.VideoProcessorSetOutputTargetRect(videoProcessor, TRUE, nullptr);

    NvidiaPpeRequest request;
    const HRESULT extension = videoContext.VideoProcessorSetStreamExtension(
        videoProcessor, 0, &kNvidiaPpeInterface, static_cast<UINT>(sizeof(request)), &request);

    if (FAILED(extension))
    {
        return std::unexpected(VsrFailure{VsrUnavailable::ExtensionRejected,
                                          describeFailure("VideoProcessorSetStreamExtension",
                                                          extension)});
    }

    processor.description =
        std::format("{}x{} {} {} -> {}x{} R8G8B8A8", picture.input.width, picture.input.height,
                    formatName(picture.format), colorSpaceName(picture.matrix, picture.range),
                    picture.output.width, picture.output.height);

    spdlog::info("VSR: NVIDIA extension accepted on the video processor ({}, PPE version 1 method "
                 "2)",
                 processor.description);

    return processor;
}

std::expected<ComPtr<ID3D11Texture2D>, std::string> createSharedTexture(
    ID3D11Device& device, SurfaceSize size, DXGI_FORMAT format, UINT bindFlags,
    std::string_view what)
{
    const D3D11_TEXTURE2D_DESC description{
        .Width = size.width,
        .Height = size.height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = format,
        .SampleDesc = {.Count = 1, .Quality = 0},
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = bindFlags,
        .CPUAccessFlags = 0,
        .MiscFlags = static_cast<UINT>(D3D11_RESOURCE_MISC_SHARED_NTHANDLE),
    };

    ComPtr<ID3D11Texture2D> texture;

    if (const HRESULT result = device.CreateTexture2D(&description, nullptr,
                                                      texture.GetAddressOf());
        FAILED(result))
    {
        return std::unexpected(describeFailure(what, result));
    }

    return texture;
}

std::expected<ComPtr<ID3D12Resource>, std::string> shareTexture(ID3D11Texture2D& texture,
                                                               ID3D12Device& device,
                                                               std::string_view what)
{
    ComPtr<IDXGIResource1> resource;

    if (const HRESULT result = texture.QueryInterface(IID_PPV_ARGS(&resource)); FAILED(result))
    {
        return std::unexpected(describeFailure(what, result));
    }

    HANDLE handle = nullptr;

    if (const HRESULT result = resource->CreateSharedHandle(
            nullptr,
            static_cast<DWORD>(DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE),
            nullptr, &handle);
        FAILED(result))
    {
        return std::unexpected(describeFailure(what, result));
    }

    ComPtr<ID3D12Resource> shared;
    const HRESULT result = device.OpenSharedHandle(handle, IID_PPV_ARGS(&shared));
    CloseHandle(handle);

    if (FAILED(result))
    {
        return std::unexpected(describeFailure(what, result));
    }

    return shared;
}

} // namespace

bool videoSuperResolutionEligible(bool enabled, std::uint32_t vendorId, SurfaceSize input,
                                  SurfaceSize output)
{
    const bool sized = input.width > 0 && input.height > 0 && output.width > 0 &&
                       output.height > 0;
    const bool upscalesOrKeeps = output.width >= input.width && output.height >= input.height;

    return enabled && vendorId == kNvidiaVendorId && sized && upscalesOrKeeps;
}

VsrAvailability VideoSuperResolution::probe()
{
    const auto adapter = findStreamAdapter();

    if (!adapter)
    {
        return {
            .available = false,
            .reason = VsrUnavailable::NoNvidiaGpu,
            .detail = adapter.error(),
        };
    }

    if (adapter->info.vendorId != kNvidiaVendorId)
    {
        return {
            .available = false,
            .reason = VsrUnavailable::NoNvidiaGpu,
            .detail = std::format("the stream runs on {} (vendor 0x{:04X})", adapter->info.name,
                                  adapter->info.vendorId),
        };
    }

    auto video = openVideoDevice(*adapter->adapter.Get());

    if (!video)
    {
        return {.available = false, .reason = video.error().reason, .detail = video.error().detail};
    }

    const VsrPicture picture{
        .input = kProbeInput,
        .output = kProbeOutput,
        .format = PixelFormat::Nv12,
        .matrix = ColorMatrix::Bt709,
        .range = ColorRange::Limited,
    };
    auto processor =
        createProcessor(*video->videoDevice.Get(), *video->videoContext.Get(), picture);

    if (!processor)
    {
        return {
            .available = false,
            .reason = processor.error().reason,
            .detail = processor.error().detail,
        };
    }

    return {
        .available = true,
        .reason = VsrUnavailable::None,
        .detail = std::format("NVIDIA extension accepted ({})", processor->description),
    };
}

std::expected<VideoSuperResolution, std::string> VideoSuperResolution::create(
    ID3D12Device& device12, IDXGIAdapter4& adapter, UINT slotCount)
{
    auto video = openVideoDevice(adapter);

    if (!video)
    {
        return std::unexpected(video.error().detail);
    }

    VideoSuperResolution vsr;
    vsr.device12_ = &device12;
    vsr.device_ = std::move(video->device);
    vsr.context_ = std::move(video->context);
    vsr.device5_ = std::move(video->device5);
    vsr.context4_ = std::move(video->context4);
    vsr.videoDevice_ = std::move(video->videoDevice);
    vsr.videoContext_ = std::move(video->videoContext);
    vsr.slotCount_ = slotCount;

    if (const HRESULT result =
            device12.CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&vsr.fence12_));
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateFence (VSR)", result));
    }

    HANDLE handle = nullptr;

    if (const HRESULT result = device12.CreateSharedHandle(vsr.fence12_.Get(), nullptr,
                                                           GENERIC_ALL, nullptr, &handle);
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateSharedHandle (VSR fence)", result));
    }

    const HRESULT opened = vsr.device5_->OpenSharedFence(handle, IID_PPV_ARGS(&vsr.fence11_));
    CloseHandle(handle);

    if (FAILED(opened))
    {
        return std::unexpected(describeFailure("OpenSharedFence", opened));
    }

    return vsr;
}

bool VideoSuperResolution::configuredFor(const VsrPicture& picture) const
{
    return configured_ && *configured_ == picture;
}

std::expected<void, std::string> VideoSuperResolution::configure(const VsrPicture& picture)
{
    if (configuredFor(picture))
    {
        return {};
    }

    const bool empty = picture.input.width == 0 || picture.input.height == 0 ||
                       picture.output.width == 0 || picture.output.height == 0;

    if (empty)
    {
        return std::unexpected(std::string("the picture has no size"));
    }

    slots_.clear();
    processor_.Reset();
    enumerator_.Reset();
    configured_.reset();
    description_.clear();

    auto processor = createProcessor(*videoDevice_.Get(), *videoContext_.Get(), picture);

    if (!processor)
    {
        return std::unexpected(processor.error().detail);
    }

    enumerator_ = std::move(processor->enumerator);
    processor_ = std::move(processor->processor);
    description_ = std::move(processor->description);

    slots_.resize(slotCount_);

    for (UINT index = 0; index < slotCount_; ++index)
    {
        auto slot = createSlot(picture);

        if (!slot)
        {
            slots_.clear();
            processor_.Reset();
            enumerator_.Reset();
            description_.clear();
            return std::unexpected(slot.error());
        }

        slots_.at(index) = std::move(*slot);
    }

    configured_ = picture;
    spdlog::info("VSR: {} shared input and output textures ready", slotCount_);
    return {};
}

std::expected<VideoSuperResolution::Slot, std::string> VideoSuperResolution::createSlot(
    const VsrPicture& picture)
{
    Slot slot;

    auto inputTexture = createSharedTexture(
        *device_.Get(), picture.input, inputFormatOf(picture.format),
        static_cast<UINT>(D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE),
        "CreateTexture2D (VSR input)");

    if (!inputTexture)
    {
        return std::unexpected(inputTexture.error());
    }

    slot.inputTexture = std::move(*inputTexture);

    auto input = shareTexture(*slot.inputTexture.Get(), *device12_.Get(), "OpenSharedHandle (VSR "
                                                                         "input)");

    if (!input)
    {
        return std::unexpected(input.error());
    }

    slot.input = std::move(*input);

    const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDescription{
        .FourCC = 0,
        .ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D,
        .Texture2D = {.MipSlice = 0, .ArraySlice = 0},
    };

    if (const HRESULT result = videoDevice_->CreateVideoProcessorInputView(
            slot.inputTexture.Get(), enumerator_.Get(), &inputDescription,
            slot.inputView.GetAddressOf());
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateVideoProcessorInputView", result));
    }

    auto outputTexture =
        createSharedTexture(*device_.Get(), picture.output, kOutputFormat,
                            static_cast<UINT>(D3D11_BIND_RENDER_TARGET),
                            "CreateTexture2D (VSR output)");

    if (!outputTexture)
    {
        return std::unexpected(outputTexture.error());
    }

    slot.outputTexture = std::move(*outputTexture);

    auto output = shareTexture(*slot.outputTexture.Get(), *device12_.Get(),
                               "OpenSharedHandle (VSR output)");

    if (!output)
    {
        return std::unexpected(output.error());
    }

    slot.output = std::move(*output);

    const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDescription{
        .ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D,
        .Texture2D = {.MipSlice = 0},
    };

    if (const HRESULT result = videoDevice_->CreateVideoProcessorOutputView(
            slot.outputTexture.Get(), enumerator_.Get(), &outputDescription,
            slot.outputView.GetAddressOf());
        FAILED(result))
    {
        return std::unexpected(describeFailure("CreateVideoProcessorOutputView", result));
    }

    return slot;
}

ID3D12Resource* VideoSuperResolution::input(UINT slot) const
{
    return slot < slots_.size() ? slots_.at(slot).input.Get() : nullptr;
}

ID3D12Resource* VideoSuperResolution::output(UINT slot) const
{
    return slot < slots_.size() ? slots_.at(slot).output.Get() : nullptr;
}

ID3D12Fence* VideoSuperResolution::fence() const
{
    return fence12_.Get();
}

UINT64 VideoSuperResolution::inputReadyValue()
{
    return ++nextFenceValue_;
}

std::expected<UINT64, std::string> VideoSuperResolution::blit(UINT slot, UINT64 inputReady)
{
    if (!processor_ || slot >= slots_.size())
    {
        return std::unexpected(std::string("VSR is not configured"));
    }

    Slot& target = slots_.at(slot);
    const UINT64 outputReady = ++nextFenceValue_;

    if (const HRESULT result = context4_->Wait(fence11_.Get(), inputReady); FAILED(result))
    {
        return std::unexpected(describeFailure("ID3D11DeviceContext4::Wait", result));
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream{};
    stream.Enable = TRUE;
    stream.OutputIndex = 0;
    stream.InputFrameOrField = 0;
    stream.pInputSurface = target.inputView.Get();

    if (const HRESULT result = videoContext_->VideoProcessorBlt(
            processor_.Get(), target.outputView.Get(), 0, 1, &stream);
        FAILED(result))
    {
        return std::unexpected(describeFailure("VideoProcessorBlt", result));
    }

    if (const HRESULT result = context4_->Signal(fence11_.Get(), outputReady); FAILED(result))
    {
        return std::unexpected(describeFailure("ID3D11DeviceContext4::Signal", result));
    }

    // The D3D12 queue's wait sees the signal only once the context's commands
    // reach the scheduler.
    context_->Flush();
    return outputReady;
}

const std::string& VideoSuperResolution::description() const
{
    return description_;
}

} // namespace lynx::render
