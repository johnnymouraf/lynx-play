#include "media/VideoDecoder.h"

#include "core/RateCounter.h"
#include "core/Latency.h"
#include "core/Windows.h"
#include "render/GpuDevice.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <format>
#include <mutex>
#include <optional>
#include <string_view>

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3d12video.h>
#include <spdlog/spdlog.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d12va.h>
}

namespace lynx::media
{

namespace
{

using Clock = std::chrono::steady_clock;

constexpr auto kStatisticsPeriod = std::chrono::seconds(5);
// Far beyond a real decode (a few milliseconds); reaching it means the GPU hung.
constexpr DWORD kDecodeTimeoutMilliseconds = 2000;
// Larger access units are not video a console or a test file sends.
constexpr std::size_t kMaxAccessUnitBytes = 64 * 1024 * 1024;

struct CodecContextDeleter
{
    void operator()(AVCodecContext* context) const noexcept
    {
        avcodec_free_context(&context);
    }
};

struct PacketDeleter
{
    void operator()(AVPacket* packet) const noexcept
    {
        av_packet_free(&packet);
    }
};

struct FrameDeleter
{
    void operator()(AVFrame* frame) const noexcept
    {
        av_frame_free(&frame);
    }
};

struct BufferDeleter
{
    void operator()(AVBufferRef* buffer) const noexcept
    {
        av_buffer_unref(&buffer);
    }
};

using CodecContextPtr = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
using PacketPtr = std::unique_ptr<AVPacket, PacketDeleter>;
using FramePtr = std::unique_ptr<AVFrame, FrameDeleter>;
using BufferPtr = std::unique_ptr<AVBufferRef, BufferDeleter>;

// What the log says about the stream, again whenever it changes.
struct StreamFormat
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    render::PixelFormat pixelFormat = render::PixelFormat::Nv12;
    render::ColorMatrix matrix = render::ColorMatrix::Bt709;
    render::ColorRange range = render::ColorRange::Limited;

    bool operator==(const StreamFormat&) const = default;
};

std::string describeAvError(std::string_view what, int result)
{
    std::array<char, AV_ERROR_MAX_STRING_SIZE> text{};
    av_strerror(result, text.data(), text.size());

    return std::format("{} failed: {}", what, text.data());
}

void forwardFfmpegLog(void* object, int level, const char* format, va_list arguments)
{
    if (level > AV_LOG_WARNING)
    {
        return;
    }

    std::array<char, 1024> line{};
    int printPrefix = 1;
    av_log_format_line2(object, level, format, arguments, line.data(),
                        static_cast<int>(line.size()), &printPrefix);

    std::string_view text(line.data());

    while (text.ends_with('\n'))
    {
        text.remove_suffix(1);
    }

    const auto severity = level <= AV_LOG_ERROR ? spdlog::level::err : spdlog::level::warn;
    spdlog::log(severity, "[ffmpeg] {}", text);
}

void forwardFfmpegLogOnce()
{
    static std::once_flag forwarding;
    std::call_once(forwarding, [] { av_log_set_callback(&forwardFfmpegLog); });
}

AVPixelFormat chooseD3d12(AVCodecContext*, const AVPixelFormat* offered)
{
    for (const AVPixelFormat* format = offered; *format != AV_PIX_FMT_NONE; ++format)
    {
        if (*format == AV_PIX_FMT_D3D12)
        {
            return *format;
        }
    }

    spdlog::error("D3D12VA can't decode this stream, and decoding on the CPU is off");
    return AV_PIX_FMT_NONE;
}

bool supportsD3d12(const AVCodec& decoder)
{
    for (int index = 0;; ++index)
    {
        const AVCodecHWConfig* config = avcodec_get_hw_config(&decoder, index);

        if (config == nullptr)
        {
            return false;
        }

        const bool usesDevice = (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0;

        if (config->device_type == AV_HWDEVICE_TYPE_D3D12VA && usesDevice)
        {
            return true;
        }
    }
}

// A D3D12VA device context on the renderer's device, so decoded pictures are
// textures the renderer can draw without a copy.
std::expected<BufferPtr, std::string> createDeviceContext(const render::GpuDevice& gpu)
{
    BufferPtr device(av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D12VA));

    if (!device)
    {
        return std::unexpected(std::string("av_hwdevice_ctx_alloc failed"));
    }

    auto* hardware = reinterpret_cast<AVHWDeviceContext*>(device->data);
    auto* d3d12 = static_cast<AVD3D12VADeviceContext*>(hardware->hwctx);

    // FFmpeg releases the device along with its context.
    gpu.device->AddRef();
    d3d12->device = gpu.device.Get();

    if (const int result = av_hwdevice_ctx_init(device.get()); result < 0)
    {
        return std::unexpected(describeAvError("av_hwdevice_ctx_init (D3D12VA)", result));
    }

    return device;
}

render::ColorMatrix matrixOf(AVColorSpace space)
{
    switch (space)
    {
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
        return render::ColorMatrix::Bt601;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        return render::ColorMatrix::Bt2020;
    default:
        // Untagged HD video, the console's included, is BT.709.
        return render::ColorMatrix::Bt709;
    }
}

StreamFormat formatOf(const AVFrame& frame)
{
    const auto* frames = reinterpret_cast<const AVHWFramesContext*>(frame.hw_frames_ctx->data);
    const bool tenBit = frames->sw_format == AV_PIX_FMT_P010;
    const bool fullRange = frame.color_range == AVCOL_RANGE_JPEG;

    return {
        .width = static_cast<std::uint32_t>(frame.width),
        .height = static_cast<std::uint32_t>(frame.height),
        .pixelFormat = tenBit ? render::PixelFormat::P010 : render::PixelFormat::Nv12,
        .matrix = matrixOf(frame.colorspace),
        .range = fullRange ? render::ColorRange::Full : render::ColorRange::Limited,
    };
}

std::string_view matrixName(render::ColorMatrix matrix)
{
    switch (matrix)
    {
    case render::ColorMatrix::Bt601:
        return "BT.601";
    case render::ColorMatrix::Bt709:
        return "BT.709";
    case render::ColorMatrix::Bt2020:
        return "BT.2020";
    }

    return "BT.709";
}

void logFormat(const StreamFormat& format)
{
    spdlog::info("Video stream: {}x{}, {}, {}, {} range", format.width, format.height,
                 format.pixelFormat == render::PixelFormat::P010 ? "P010" : "NV12",
                 matrixName(format.matrix),
                 format.range == render::ColorRange::Full ? "full" : "limited");
}

} // namespace

struct VideoDecoder::Context
{
    core::VideoCodec codec = core::VideoCodec::H265;
    std::shared_ptr<render::FrameMailbox> mailbox;
    std::shared_ptr<core::StreamMetrics> metrics;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    CodecContextPtr codecContext;
    PacketPtr packet;
    FramePtr frame;
    // Set when the GPU finishes a picture.
    core::UniqueHandle decodeDone;
    core::RateCounter decoded{Clock::now(), kStatisticsPeriod};
    std::uint64_t errors = 0;
    std::uint64_t totalFrames = 0;
    std::optional<StreamFormat> format;

    [[nodiscard]] std::expected<void, std::string> send(std::span<const std::uint8_t> accessUnit);
    [[nodiscard]] std::expected<void, std::string> receive(Clock::time_point sent);
    [[nodiscard]] std::expected<void, std::string> publish(const AVFrame& picture,
                                                           Clock::time_point sent);
    [[nodiscard]] std::expected<void, std::string> waitForGpu(const AVD3D12VASyncContext& sync);
    void reportStatistics();
};

std::expected<std::unique_ptr<VideoDecoder>, std::string> VideoDecoder::create(
    const render::GpuDevice& gpu, core::VideoCodec codec,
    std::shared_ptr<render::FrameMailbox> mailbox, std::shared_ptr<core::StreamMetrics> metrics)
{
    forwardFfmpegLogOnce();

    const AVCodecID codecId = codec == core::VideoCodec::H265 ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
    const AVCodec* decoder = avcodec_find_decoder(codecId);

    if (decoder == nullptr || !supportsD3d12(*decoder))
    {
        return std::unexpected(
            std::format("FFmpeg has no {} decoder with D3D12VA", core::codecName(codec)));
    }

    auto device = createDeviceContext(gpu);

    if (!device)
    {
        return std::unexpected(device.error());
    }

    auto context = std::make_unique<Context>();
    context->codec = codec;
    context->mailbox = std::move(mailbox);
    context->metrics = std::move(metrics);
    context->dxgiDevice = gpu.dxgiDevice;
    context->codecContext.reset(avcodec_alloc_context3(decoder));
    context->packet.reset(av_packet_alloc());
    context->frame.reset(av_frame_alloc());
    context->decodeDone.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));

    if (!context->codecContext || !context->packet || !context->frame || !context->decodeDone)
    {
        return std::unexpected(std::string("out of memory for the decoder"));
    }

    AVCodecContext& codecContext = *context->codecContext;
    codecContext.hw_device_ctx = av_buffer_ref(device->get());
    codecContext.get_format = &chooseD3d12;
    // Output each picture as soon as it is decoded, and decode on one thread:
    // frame threading would hold pictures back.
    codecContext.flags |= AV_CODEC_FLAG_LOW_DELAY;
    codecContext.thread_count = 1;

    if (const int result = avcodec_open2(&codecContext, decoder, nullptr); result < 0)
    {
        return std::unexpected(describeAvError("avcodec_open2", result));
    }

    if (context->metrics != nullptr)
    {
        context->metrics->setCodec(codec);
    }

    spdlog::info("Decoding {} on the GPU with D3D12VA (hardware decoding)", core::codecName(codec));
    spdlog::info("Decoder latency: low-delay output, one thread, no frame threading (flags reviewed)");
    return std::unique_ptr<VideoDecoder>(new VideoDecoder(std::move(context)));
}

VideoDecoder::VideoDecoder(std::unique_ptr<Context> context) : context_(std::move(context))
{
}

VideoDecoder::~VideoDecoder()
{
    spdlog::info("{} decoder closed after {} frames", core::codecName(context_->codec),
                 context_->totalFrames);
}

std::expected<void, std::string> VideoDecoder::decode(std::span<const std::uint8_t> accessUnit)
{
    static thread_local bool latencySetup = false;
    static thread_local std::optional<core::MmcssRegistration> mmcss;

    if (!latencySetup)
    {
        latencySetup = true;
        SetThreadDescription(GetCurrentThread(), L"Lynx video decoder");
        mmcss.emplace(core::MmcssRegistration::enable(L"Games"));

        if (context_->dxgiDevice)
        {
            render::raiseCurrentThreadGpuPriority(*context_->dxgiDevice.Get());
        }
    }

    const Clock::time_point sent = Clock::now();

    if (context_->metrics != nullptr)
    {
        context_->metrics->addReceived(sent, accessUnit.size());
    }

    auto decoded = context_->send(accessUnit).and_then([&] { return context_->receive(sent); });

    if (!decoded)
    {
        ++context_->errors;
    }

    context_->reportStatistics();
    return decoded;
}

std::expected<void, std::string> VideoDecoder::Context::send(
    std::span<const std::uint8_t> accessUnit)
{
    if (accessUnit.empty() || accessUnit.size() > kMaxAccessUnitBytes)
    {
        return std::unexpected(std::format("an access unit of {} bytes", accessUnit.size()));
    }

    // A buffer of FFmpeg's own, with the zeroed padding its bitstream readers
    // need past the end.
    av_packet_unref(packet.get());

    if (av_new_packet(packet.get(), static_cast<int>(accessUnit.size())) < 0)
    {
        return std::unexpected(std::string("out of memory for a packet"));
    }

    std::ranges::copy(accessUnit, packet->data);

    if (const int result = avcodec_send_packet(codecContext.get(), packet.get()); result < 0)
    {
        return std::unexpected(describeAvError("avcodec_send_packet", result));
    }

    return {};
}

std::expected<void, std::string> VideoDecoder::Context::receive(Clock::time_point sent)
{
    while (true)
    {
        const int result = avcodec_receive_frame(codecContext.get(), frame.get());

        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
        {
            return {};
        }

        if (result < 0)
        {
            return std::unexpected(describeAvError("avcodec_receive_frame", result));
        }

        auto published = publish(*frame, sent);
        av_frame_unref(frame.get());

        if (!published)
        {
            return published;
        }
    }
}

std::expected<void, std::string> VideoDecoder::Context::publish(const AVFrame& picture,
                                                                 Clock::time_point sent)
{
    if (picture.format != AV_PIX_FMT_D3D12)
    {
        return std::unexpected(std::string("the decoder returned a picture outside the GPU"));
    }

    const auto& surface = *reinterpret_cast<const AVD3D12VAFrame*>(picture.data[0]);

    // Waiting here times the decode, and the renderer only ever gets finished
    // pictures, so its queue never stalls on one.
    if (auto finished = waitForGpu(surface.sync_ctx); !finished)
    {
        return finished;
    }

    const Clock::time_point now = Clock::now();
    const double milliseconds =
        std::chrono::duration<double, std::milli>(now - sent).count();
    decoded.add(now - sent);
    ++totalFrames;

    if (metrics != nullptr)
    {
        metrics->addDecoded(now, milliseconds);
    }

    const StreamFormat current = formatOf(picture);

    if (format != current)
    {
        logFormat(current);
        format = current;

        if (metrics != nullptr)
        {
            metrics->setFormat(current.width, current.height);
        }
    }

    // The renderer holds a reference to the picture until it is done reading
    // it, which keeps the decoder from writing into that surface.
    FramePtr kept(av_frame_clone(&picture));

    if (!kept)
    {
        return std::unexpected(std::string("out of memory for a picture"));
    }

    mailbox->publish({
        .texture = surface.texture,
        .arraySlice = static_cast<std::uint32_t>(surface.subresource_index),
        .format = current.pixelFormat,
        .matrix = current.matrix,
        .range = current.range,
        .width = current.width,
        .height = current.height,
        .fence = surface.sync_ctx.fence,
        .fenceValue = surface.sync_ctx.fence_value,
        .timestamp = now,
        .lease = std::shared_ptr<const AVFrame>(std::move(kept)),
    });

    return {};
}

std::expected<void, std::string> VideoDecoder::Context::waitForGpu(
    const AVD3D12VASyncContext& sync)
{
    if (sync.fence->GetCompletedValue() >= sync.fence_value)
    {
        return {};
    }

    sync.fence->SetEventOnCompletion(sync.fence_value, decodeDone.get());

    if (WaitForSingleObject(decodeDone.get(), kDecodeTimeoutMilliseconds) != WAIT_OBJECT_0)
    {
        return std::unexpected(std::string("the GPU didn't finish decoding a picture in time"));
    }

    return {};
}

void VideoDecoder::Context::reportStatistics()
{
    const auto summary = decoded.takeIfDue(Clock::now());

    if (!summary)
    {
        return;
    }

    spdlog::info("Decoded {:.1f} fps with D3D12VA, {:.2f} ms per frame on average ({:.2f} ms at "
                 "most), {} errors",
                 summary->perSecond, summary->averageMilliseconds, summary->maxMilliseconds,
                 errors);
    errors = 0;
}

} // namespace lynx::media
