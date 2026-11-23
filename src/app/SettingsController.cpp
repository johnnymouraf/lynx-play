#include "app/SettingsController.h"

#include "audio/Devices.h"
#include "core/KeyboardBindings.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

#include <windows.h>

#include <spdlog/spdlog.h>

namespace lynx::app
{

namespace
{

using namespace Qt::StringLiterals;
using core::PresentMode;
using core::VideoCodec;
using core::VideoResolution;

constexpr int kKbpsPerMbps = 1000;

struct ResolutionLines
{
    VideoResolution resolution;
    int lines;
};

constexpr std::array<ResolutionLines, 4> kResolutionLines{{
    {VideoResolution::P360, 360},
    {VideoResolution::P540, 540},
    {VideoResolution::P720, 720},
    {VideoResolution::P1080, 1080},
}};

int linesOf(VideoResolution resolution)
{
    const auto found =
        std::ranges::find(kResolutionLines, resolution, &ResolutionLines::resolution);

    return found != kResolutionLines.end() ? found->lines : 1080;
}

std::string_view presentModeName(PresentMode mode)
{
    return mode == PresentMode::LowestLatency ? "lowest latency" : "VSync";
}

std::uint16_t virtualKeyOf(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
    {
        return static_cast<std::uint16_t>(key);
    }

    if (key >= Qt::Key_0 && key <= Qt::Key_9)
    {
        return static_cast<std::uint16_t>(key);
    }

    switch (key)
    {
    case Qt::Key_Space:
        return VK_SPACE;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return VK_RETURN;
    case Qt::Key_Tab:
        return VK_TAB;
    case Qt::Key_Backspace:
        return VK_BACK;
    case Qt::Key_Escape:
        return VK_ESCAPE;
    case Qt::Key_Shift:
        return VK_SHIFT;
    case Qt::Key_Control:
        return VK_CONTROL;
    case Qt::Key_Alt:
        return VK_MENU;
    case Qt::Key_Up:
        return VK_UP;
    case Qt::Key_Down:
        return VK_DOWN;
    case Qt::Key_Left:
        return VK_LEFT;
    case Qt::Key_Right:
        return VK_RIGHT;
    case Qt::Key_Home:
        return VK_HOME;
    case Qt::Key_End:
        return VK_END;
    case Qt::Key_PageUp:
        return VK_PRIOR;
    case Qt::Key_PageDown:
        return VK_NEXT;
    case Qt::Key_Insert:
        return VK_INSERT;
    case Qt::Key_Delete:
        return VK_DELETE;
    case Qt::Key_Semicolon:
        return VK_OEM_1;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        return VK_OEM_PLUS;
    case Qt::Key_Comma:
        return VK_OEM_COMMA;
    case Qt::Key_Minus:
        return VK_OEM_MINUS;
    case Qt::Key_Period:
        return VK_OEM_PERIOD;
    case Qt::Key_Slash:
        return VK_OEM_2;
    case Qt::Key_QuoteLeft:
        return VK_OEM_3;
    case Qt::Key_BracketLeft:
        return VK_OEM_4;
    case Qt::Key_Backslash:
        return VK_OEM_5;
    case Qt::Key_BracketRight:
        return VK_OEM_6;
    case Qt::Key_Apostrophe:
        return VK_OEM_7;
    case Qt::Key_F1:
        return VK_F1;
    case Qt::Key_F2:
        return VK_F2;
    case Qt::Key_F3:
        return VK_F3;
    case Qt::Key_F4:
        return VK_F4;
    case Qt::Key_F5:
        return VK_F5;
    case Qt::Key_F6:
        return VK_F6;
    case Qt::Key_F7:
        return VK_F7;
    case Qt::Key_F8:
        return VK_F8;
    case Qt::Key_F9:
        return VK_F9;
    case Qt::Key_F10:
        return VK_F10;
    case Qt::Key_F11:
        return VK_F11;
    case Qt::Key_F12:
        return VK_F12;
    default:
        return 0;
    }
}

} // namespace

SettingsController::SettingsController(std::optional<core::SettingsStore> store, QObject* parent)
    : QObject(parent), store_(std::move(store))
{
    if (store_)
    {
        settings_ = store_->load();
    }

    if (settings_.rtx.frameGeneration == core::FrameGenerationMode::X2 &&
        settings_.rtx.reflex == core::ReflexMode::Off)
    {
        settings_.rtx.reflex = core::ReflexMode::On;
        spdlog::info("Frame Generation requires Reflex; Reflex turned on");
        save();
    }

    reflexAvailability_ = render::ReflexSession::probe();

    if (reflexAvailability_.available)
    {
        spdlog::info("Reflex is available ({})", reflexAvailability_.detail);
    }
    else
    {
        spdlog::info("Reflex is unavailable: {}", reflexAvailability_.detail);
    }

    videoSuperResolutionAvailability_ = render::VideoSuperResolution::probe();

    if (videoSuperResolutionAvailability_.available)
    {
        spdlog::info("VSR is available ({})", videoSuperResolutionAvailability_.detail);
    }
    else
    {
        spdlog::info("VSR is unavailable: {}", videoSuperResolutionAvailability_.detail);
    }

    opticalFlowAvailability_ = render::OpticalFlow::probe();

    if (opticalFlowAvailability_.available)
    {
        spdlog::info("Optical flow is available ({})", opticalFlowAvailability_.detail);
    }
    else
    {
        spdlog::info("Optical flow is unavailable: {}", opticalFlowAvailability_.detail);
    }

    dlssAvailability_ = render::probeDlss();

    if (dlssAvailability_.available)
    {
        spdlog::info("DLSS SR is available ({})", dlssAvailability_.detail);
    }
    else
    {
        spdlog::info("DLSS SR is unavailable: {}", dlssAvailability_.detail);
    }

    frameGenerationAvailability_ = render::FrameGeneration::probe();

    if (frameGenerationAvailability_.available)
    {
        spdlog::info("Frame Generation is available ({})", frameGenerationAvailability_.detail);
    }
    else
    {
        spdlog::info("Frame Generation is unavailable: {}", frameGenerationAvailability_.detail);
    }

    hdrDisplay_ = render::detectHdrDisplay(nullptr);
    spdlog::info("HDR display: '{}', HDR supported {}, HDR active {}, SDR white {:.0f} nits",
                 hdrDisplay_.name.empty() ? "unknown" : hdrDisplay_.name,
                 hdrDisplay_.hdrSupported ? "yes" : "no", hdrDisplay_.hdrActive ? "yes" : "no",
                 hdrDisplay_.sdrWhiteNits);

    rtxHdrAvailability_ = render::VideoSuperResolution::probeHdr();

    if (rtxHdrAvailability_.available)
    {
        spdlog::info("RTX Video HDR is available ({})", rtxHdrAvailability_.detail);
    }
    else
    {
        spdlog::info("RTX Video HDR is unavailable: {}", rtxHdrAvailability_.detail);
    }

    spdlog::info("HDR setting: {} (RTX Video HDR {})", core::hdrModeName(settings_.video.hdr),
                 hdrRtxAvailable() ? "available on this display" : "unavailable on this display");
}

const core::VideoSettings& SettingsController::video() const
{
    return settings_.video;
}

core::HdrMode SettingsController::hdrMode() const
{
    return settings_.video.hdr;
}

bool SettingsController::rtxVideoHdrAvailable() const
{
    return rtxHdrAvailability_.available;
}

const core::AudioSettings& SettingsController::audio() const
{
    return settings_.audio;
}

const core::ControllerSettings& SettingsController::controller() const
{
    return settings_.controller;
}

const core::OverlaySettings& SettingsController::overlay() const
{
    return settings_.overlay;
}

render::PresentTiming SettingsController::presentTiming() const
{
    const bool lowest = settings_.video.presentMode == PresentMode::LowestLatency;
    return lowest ? render::PresentTiming::Immediate : render::PresentTiming::VSync;
}

int SettingsController::resolution() const
{
    return linesOf(settings_.video.resolution);
}

void SettingsController::setResolution(int lines)
{
    const auto found = std::ranges::find(kResolutionLines, lines, &ResolutionLines::lines);

    if (found == kResolutionLines.end())
    {
        return;
    }

    core::VideoSettings video = settings_.video;
    video.resolution = found->resolution;
    apply(video);
}

int SettingsController::framesPerSecond() const
{
    return settings_.video.framesPerSecond;
}

void SettingsController::setFramesPerSecond(int rate)
{
    if (rate != 30 && rate != 60)
    {
        return;
    }

    core::VideoSettings video = settings_.video;
    video.framesPerSecond = rate;
    apply(video);
}

QString SettingsController::codec() const
{
    return settings_.video.codec == VideoCodec::H265 ? u"h265"_s : u"h264"_s;
}

void SettingsController::setCodec(const QString& name)
{
    if (name != u"h264"_s && name != u"h265"_s)
    {
        return;
    }

    core::VideoSettings video = settings_.video;
    video.codec = name == u"h265"_s ? VideoCodec::H265 : VideoCodec::H264;
    apply(video);
}

int SettingsController::bitrateMbps() const
{
    return static_cast<int>(settings_.video.bitrateKbps) / kKbpsPerMbps;
}

void SettingsController::setBitrateMbps(int megabits)
{
    const int clamped = std::clamp(megabits, minBitrateMbps(), maxBitrateMbps());

    core::VideoSettings video = settings_.video;
    video.bitrateKbps = static_cast<std::uint32_t>(clamped * kKbpsPerMbps);
    apply(video);
}

bool SettingsController::lowestLatency() const
{
    return settings_.video.presentMode == PresentMode::LowestLatency;
}

void SettingsController::setLowestLatency(bool lowest)
{
    core::VideoSettings video = settings_.video;
    video.presentMode = lowest ? PresentMode::LowestLatency : PresentMode::VSync;
    apply(video);
}

QString SettingsController::hdr() const
{
    return QString::fromLatin1(core::hdrModeName(settings_.video.hdr).data());
}

void SettingsController::setHdr(const QString& name)
{
    core::HdrMode mode = core::HdrMode::Auto;

    if (name == u"off"_s)
    {
        mode = core::HdrMode::Off;
    }
    else if (name == u"auto"_s)
    {
        mode = core::HdrMode::Auto;
    }
    else if (name == u"rtx"_s)
    {
        mode = core::HdrMode::RtxVideoHdr;
    }
    else
    {
        return;
    }

    core::VideoSettings video = settings_.video;
    video.hdr = mode;
    apply(video);
}

QString SettingsController::hdrDescription() const
{
    QString description;

    if (!hdrDisplay_.hdrSupported)
    {
        description = u"Esta tela não é HDR: o HDR do PS5 é convertido para SDR."_s;
    }
    else if (!hdrDisplay_.hdrActive)
    {
        description = u"Esta tela é HDR, mas o HDR do Windows está desligado. Ligue em "
                      u"Configurações › Sistema › Tela › HDR para ver HDR."_s;
    }
    else
    {
        description = u"Tela HDR ativa (%1)."_s.arg(QString::fromStdString(hdrDisplay_.name));
    }

    if (hdrRtxAvailable())
    {
        description +=
            u" O RTX Video HDR também precisa estar ligado no NVIDIA App (Sistema › Vídeo › "
            u"RTX Video Enhancements)."_s;
    }
    else
    {
        description += u" RTX Video HDR indisponível: "_s +
                       (rtxHdrAvailability_.available
                            ? u"requer uma tela HDR com o HDR do Windows ligado."_s
                            : u"o driver NVIDIA não aceitou a extensão."_s);
    }

    description += u" A escolha vale para a próxima transmissão."_s;
    return description;
}

bool SettingsController::hdrRtxAvailable() const
{
    return hdrDisplay_.hdrActive && rtxHdrAvailability_.available;
}

QString SettingsController::windowMode() const
{
    return QString::fromLatin1(core::windowModeName(settings_.video.windowMode).data());
}

void SettingsController::setWindowMode(const QString& name)
{
    core::VideoSettings video = settings_.video;

    if (name == u"fullscreen"_s)
    {
        video.windowMode = core::WindowMode::Fullscreen;
    }
    else if (name == u"windowed"_s)
    {
        video.windowMode = core::WindowMode::Windowed;
    }
    else
    {
        return;
    }

    apply(video);
}

QString SettingsController::windowSizePreset() const
{
    const auto preset = core::windowSizePresetIndex(settings_.video.windowSize);

    if (!preset)
    {
        return u"custom"_s;
    }

    return QString::fromLatin1(core::kWindowSizePresetNames.at(*preset).data());
}

void SettingsController::setWindowSizePreset(const QString& name)
{
    for (std::size_t index = 0; index < core::kWindowSizePresetNames.size(); ++index)
    {
        if (name == QString::fromLatin1(core::kWindowSizePresetNames.at(index).data()))
        {
            setWindowedSize(core::kWindowSizePresets.at(index));
            return;
        }
    }
}

QString SettingsController::windowSizeText() const
{
    return u"%1 × %2"_s.arg(settings_.video.windowSize.width)
        .arg(settings_.video.windowSize.height);
}

void SettingsController::setWindowedSize(core::WindowSize size)
{
    core::VideoSettings video = settings_.video;
    video.windowSize = core::clampWindowSize(size, core::kMaxWindowSize);
    apply(video);
}

void SettingsController::setWindowLayout(core::WindowMode mode, core::WindowSize size)
{
    core::VideoSettings video = settings_.video;
    video.windowMode = mode;
    video.windowSize = core::clampWindowSize(size, core::kMaxWindowSize);
    apply(video);
}

int SettingsController::minBitrateMbps() const
{
    return static_cast<int>(core::kMinBitrateKbps) / kKbpsPerMbps;
}

int SettingsController::maxBitrateMbps() const
{
    return static_cast<int>(core::kMaxBitrateKbps) / kKbpsPerMbps;
}

void SettingsController::refreshAudioDevices()
{
    const std::vector<audio::AudioDeviceInfo> outputs = audio::outputDevices();
    const std::vector<audio::AudioDeviceInfo> microphones = audio::inputDevices();
    QStringList outputNames;
    QStringList microphoneNames;

    for (const audio::AudioDeviceInfo& device : outputs)
    {
        outputNames << QString::fromStdString(device.name);
    }

    for (const audio::AudioDeviceInfo& device : microphones)
    {
        microphoneNames << QString::fromStdString(device.name);
    }

    const bool changed =
        outputNames != outputDevices_ || microphoneNames != microphoneDevices_;
    outputDevices_ = outputNames;
    microphoneDevices_ = microphoneNames;

    if (!devicesLogged_ || changed)
    {
        for (const audio::AudioDeviceInfo& device : outputs)
        {
            spdlog::info("Audio output '{}': {} Hz, {} channels", device.name, device.sampleRate,
                         device.channels);
        }

        for (const audio::AudioDeviceInfo& device : microphones)
        {
            spdlog::info("Audio input '{}': {} Hz, {} channels", device.name, device.sampleRate,
                         device.channels);
        }

        devicesLogged_ = true;
    }

    if (changed)
    {
        emit audioDevicesChanged();
    }
}

QStringList SettingsController::outputDevices() const
{
    return outputDevices_;
}

QStringList SettingsController::microphoneDevices() const
{
    return microphoneDevices_;
}

QString SettingsController::outputDevice() const
{
    return QString::fromStdString(settings_.audio.outputDevice);
}

void SettingsController::setOutputDevice(const QString& name)
{
    core::AudioSettings audio = settings_.audio;
    audio.outputDevice = name.toStdString();
    apply(audio);
}

QString SettingsController::microphoneDevice() const
{
    return QString::fromStdString(settings_.audio.microphoneDevice);
}

void SettingsController::setMicrophoneDevice(const QString& name)
{
    core::AudioSettings audio = settings_.audio;
    audio.microphoneDevice = name.toStdString();
    apply(audio);
}

bool SettingsController::microphoneMuted() const
{
    return settings_.audio.microphoneMuted;
}

void SettingsController::setMicrophoneMuted(bool muted)
{
    core::AudioSettings audio = settings_.audio;
    audio.microphoneMuted = muted;
    apply(audio);
}

int SettingsController::volumePercent() const
{
    return settings_.audio.volumePercent;
}

void SettingsController::setVolumePercent(int percent)
{
    core::AudioSettings audio = settings_.audio;
    audio.volumePercent = std::clamp(percent, 0, 100);
    apply(audio);
}

QString SettingsController::preferredController() const
{
    return QString::fromStdString(settings_.controller.preferred);
}

void SettingsController::setPreferredController(const QString& name)
{
    core::ControllerSettings controllerSettings;
    controllerSettings.preferred = name.toStdString();
    apply(controllerSettings);
}

core::ReflexMode SettingsController::reflexMode() const
{
    return settings_.rtx.reflex;
}

QString SettingsController::reflex() const
{
    switch (settings_.rtx.reflex)
    {
    case core::ReflexMode::Off:
        return u"off"_s;
    case core::ReflexMode::On:
        return u"on"_s;
    case core::ReflexMode::Boost:
        return u"boost"_s;
    }

    return u"off"_s;
}

void SettingsController::setReflex(const QString& name)
{
    core::RtxSettings rtx = settings_.rtx;

    if (name == u"off"_s)
    {
        rtx.reflex = core::ReflexMode::Off;
    }
    else if (name == u"on"_s)
    {
        rtx.reflex = core::ReflexMode::On;
    }
    else if (name == u"boost"_s)
    {
        rtx.reflex = core::ReflexMode::Boost;
    }
    else
    {
        return;
    }

    if (rtx.reflex == core::ReflexMode::Off &&
        rtx.frameGeneration == core::FrameGenerationMode::X2)
    {
        rtx.frameGeneration = core::FrameGenerationMode::Off;
        spdlog::info("Frame Generation turned off: it requires Reflex");
    }

    applyRtx(rtx);
}

bool SettingsController::reflexAvailable() const
{
    return reflexAvailability_.available;
}

QString SettingsController::reflexUnavailableReason() const
{
    switch (reflexAvailability_.reason)
    {
    case render::ReflexUnavailable::NoNvidiaGpu:
        return u"A GPU usada para o vídeo não é NVIDIA."_s;
    case render::ReflexUnavailable::NoDriver:
        return u"O driver NVIDIA não está instalado."_s;
    case render::ReflexUnavailable::DriverFailed:
        return u"O driver NVIDIA não respondeu ao NVAPI."_s;
    case render::ReflexUnavailable::None:
        break;
    }

    return {};
}

core::Upscaling SettingsController::upscalingMode() const
{
    return settings_.rtx.upscaling;
}

QString SettingsController::upscaling() const
{
    return QString::fromLatin1(core::upscalingName(settings_.rtx.upscaling));
}

void SettingsController::setUpscaling(const QString& name)
{
    core::RtxSettings rtx = settings_.rtx;

    if (name == u"shader"_s)
    {
        rtx.upscaling = core::Upscaling::Shader;
    }
    else if (name == u"vsr"_s)
    {
        rtx.upscaling = core::Upscaling::Vsr;
    }
    else if (name == u"dlss"_s)
    {
        rtx.upscaling = core::Upscaling::Dlss;
    }
    else
    {
        return;
    }

    applyRtx(rtx);
}

bool SettingsController::videoSuperResolutionAvailable() const
{
    return videoSuperResolutionAvailability_.available;
}

QString SettingsController::videoSuperResolutionUnavailableReason() const
{
    switch (videoSuperResolutionAvailability_.reason)
    {
    case render::VsrUnavailable::NoNvidiaGpu:
        return u"A GPU usada para o vídeo não é NVIDIA."_s;
    case render::VsrUnavailable::NoD3D11:
        return u"O driver de vídeo não expôs a interface D3D11 necessária."_s;
    case render::VsrUnavailable::FormatUnsupported:
        return u"O driver não converte o vídeo NV12 para RGB."_s;
    case render::VsrUnavailable::ProcessorFailed:
        return u"O driver não criou o processador de vídeo."_s;
    case render::VsrUnavailable::ExtensionRejected:
        return u"O driver não aceitou a extensão do RTX Video Super Resolution."_s;
    case render::VsrUnavailable::None:
        break;
    }

    return {};
}

bool SettingsController::dlssAvailable() const
{
    return dlssAvailability_.available && settings_.rtx.opticalFlow &&
           opticalFlowAvailability_.available;
}

QString SettingsController::dlssUnavailableReason() const
{
    if (!settings_.rtx.opticalFlow)
    {
        return u"O Optical Flow está desligado; o DLSS usa os vetores de movimento."_s;
    }

    if (!opticalFlowAvailability_.available)
    {
        return u"O Optical Flow está indisponível."_s;
    }

    switch (dlssAvailability_.reason)
    {
    case render::DlssUnavailable::NoNvidiaGpu:
        return u"A GPU usada para o vídeo não é NVIDIA."_s;
    case render::DlssUnavailable::NoOpticalFlow:
        return u"O Optical Flow está indisponível."_s;
    case render::DlssUnavailable::GpuTooOld:
        return u"O DLSS Super Resolution requer uma GPU Ada ou mais nova."_s;
    case render::DlssUnavailable::ArchitectureUnknown:
        return u"Não foi possível identificar a arquitetura da GPU."_s;
    case render::DlssUnavailable::NoNgxCore:
        return u"O driver NVIDIA não trouxe o NGX (nvngx.dll)."_s;
    case render::DlssUnavailable::NoDlssRuntime:
        return u"O nvngx_dlss.dll não está na pasta do app."_s;
    case render::DlssUnavailable::InitFailed:
        return u"O NGX não inicializou com a GPU da transmissão."_s;
    case render::DlssUnavailable::FeatureFailed:
        return u"O DLSS não criou o recurso para esta imagem."_s;
    case render::DlssUnavailable::None:
        break;
    }

    return {};
}

bool SettingsController::opticalFlow() const
{
    return settings_.rtx.opticalFlow;
}

void SettingsController::setOpticalFlow(bool enabled)
{
    core::RtxSettings rtx = settings_.rtx;
    rtx.opticalFlow = enabled;
    applyRtx(rtx);
}

bool SettingsController::opticalFlowAvailable() const
{
    return opticalFlowAvailability_.available;
}

QString SettingsController::opticalFlowUnavailableReason() const
{
    switch (opticalFlowAvailability_.reason)
    {
    case render::OpticalFlowUnavailable::NoNvidiaGpu:
        return u"A GPU usada para o vídeo não é NVIDIA."_s;
    case render::OpticalFlowUnavailable::NoDriver:
        return u"O driver NVIDIA não traz o nvofapi64.dll."_s;
    case render::OpticalFlowUnavailable::DriverTooOld:
        return u"O driver NVIDIA é anterior ao Optical Flow 5.0."_s;
    case render::OpticalFlowUnavailable::DriverFailed:
        return u"O driver NVIDIA recusou o Optical Flow."_s;
    case render::OpticalFlowUnavailable::FormatUnsupported:
        return u"O driver não aceita o fluxo óptico no formato do vídeo."_s;
    case render::OpticalFlowUnavailable::SizeUnsupported:
        return u"A resolução do vídeo excede o limite do Optical Flow."_s;
    case render::OpticalFlowUnavailable::SessionFailed:
        return u"O fluxo óptico parou nesta transmissão."_s;
    case render::OpticalFlowUnavailable::None:
        break;
    }

    return {};
}

QString SettingsController::frameGeneration() const
{
    return QString::fromLatin1(core::frameGenerationName(settings_.rtx.frameGeneration).data());
}

core::FrameGenerationMode SettingsController::frameGenerationMode() const
{
    return settings_.rtx.frameGeneration;
}

void SettingsController::setFrameGeneration(const QString& name)
{
    core::FrameGenerationMode mode = core::FrameGenerationMode::Off;

    if (name == u"off"_s)
    {
        mode = core::FrameGenerationMode::Off;
    }
    else if (name == u"2x"_s)
    {
        mode = core::FrameGenerationMode::X2;
    }
    else
    {
        return;
    }

    core::RtxSettings rtx = settings_.rtx;
    rtx.frameGeneration = mode;

    if (mode == core::FrameGenerationMode::X2 && rtx.reflex == core::ReflexMode::Off)
    {
        rtx.reflex = core::ReflexMode::On;
        spdlog::info("Frame Generation requires Reflex; Reflex turned on");
    }

    applyRtx(rtx);
}

bool SettingsController::frameGenerationAvailable() const
{
    return frameGenerationAvailability_.available && reflexAvailability_.available &&
           settings_.rtx.opticalFlow;
}

QString SettingsController::frameGenerationUnavailableReason() const
{
    if (!frameGenerationAvailability_.available)
    {
        switch (frameGenerationAvailability_.reason)
        {
        case render::FrameGenerationUnavailable::NoNvidiaGpu:
            return u"A GPU usada para o vídeo não é NVIDIA."_s;
        case render::FrameGenerationUnavailable::RuntimeMissing:
            return u"O runtime do DLSS Frame Generation (nvngx_dlssg.dll) não está instalado."_s;
        case render::FrameGenerationUnavailable::HagsOff:
            return u"A GPU agendada por hardware do Windows está desligada (Configurações › "
                   u"Sistema › Tela › Gráficos › Alterar configuração de gráficos padrão)."_s;
        case render::FrameGenerationUnavailable::DriverTooOld:
            return u"O driver NVIDIA é anterior ao mínimo exigido pelo DLSS Frame Generation."_s;
        case render::FrameGenerationUnavailable::NotAvailable:
            return u"Esta GPU não suporta DLSS Frame Generation (requer uma RTX 40 ou mais "
                   u"nova)."_s;
        case render::FrameGenerationUnavailable::DriverFailed:
            return u"O driver NVIDIA recusou o NGX."_s;
        case render::FrameGenerationUnavailable::None:
            break;
        }
    }

    if (!reflexAvailability_.available)
    {
        return u"O NVIDIA Reflex não está disponível."_s;
    }

    if (!settings_.rtx.opticalFlow)
    {
        return u"Ligue o Optical Flow para usar o Frame Generation."_s;
    }

    return {};
}

bool SettingsController::overlayStatisticsVisible() const
{
    return settings_.overlay.statisticsVisible;
}

void SettingsController::setOverlayStatisticsVisible(bool visible)
{
    core::OverlaySettings overlay = settings_.overlay;
    overlay.statisticsVisible = visible;
    applyOverlay(overlay);
}

const core::KeyboardMouseSettings& SettingsController::keyboardMouse() const
{
    return settings_.keyboardMouse;
}

QVariantMap SettingsController::keyboardBindings() const
{
    QVariantMap bindings;

    for (std::size_t index = 0; index < core::kKeyboardControlCount; ++index)
    {
        const std::string_view name = core::kKeyboardControlNames.at(index);
        bindings.insert(QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size())),
                        QString::fromStdString(settings_.keyboardMouse.bindings.at(index)));
    }

    return bindings;
}

int SettingsController::mouseSensitivityPercent() const
{
    return static_cast<int>(std::lround(settings_.keyboardMouse.mouseSensitivity * 100.0));
}

void SettingsController::setMouseSensitivityPercent(int percent)
{
    core::KeyboardMouseSettings keyboardMouse = settings_.keyboardMouse;
    keyboardMouse.mouseSensitivity =
        static_cast<double>(std::clamp(percent, 10, 500)) / 100.0;
    applyKeyboardMouse(keyboardMouse);
}

bool SettingsController::invertMouseY() const
{
    return settings_.keyboardMouse.invertY;
}

void SettingsController::setInvertMouseY(bool invert)
{
    core::KeyboardMouseSettings keyboardMouse = settings_.keyboardMouse;
    keyboardMouse.invertY = invert;
    applyKeyboardMouse(keyboardMouse);
}

void SettingsController::recordPsnSignIn(std::optional<std::int64_t> tokenExpiry)
{
    core::PsnSettings psn;
    psn.signedIn = true;
    psn.tokenExpiry = tokenExpiry;

    if (psn == settings_.psn)
    {
        return;
    }

    settings_.psn = psn;
    spdlog::info("PSN sign-in saved with its token expiry");
    save();
}

void SettingsController::setKeyboardBinding(const QString& control, const QString& binding)
{
    std::size_t index = core::kKeyboardControlCount;

    for (std::size_t candidate = 0; candidate < core::kKeyboardControlCount; ++candidate)
    {
        const std::string_view name = core::kKeyboardControlNames.at(candidate);

        if (control == QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size())))
        {
            index = candidate;
            break;
        }
    }

    if (index == core::kKeyboardControlCount || binding.isEmpty() ||
        !core::keyboardBindingFromName(binding.toStdString()))
    {
        return;
    }

    applyKeyboardMouse(core::assignKeyboardBinding(
        settings_.keyboardMouse, static_cast<core::KeyboardControl>(index), binding.toStdString()));
}

QString SettingsController::bindingNameForKey(int qtKey) const
{
    const std::uint16_t virtualKey = virtualKeyOf(qtKey);

    if (virtualKey == 0)
    {
        return {};
    }

    return QString::fromStdString(core::keyboardBindingName(
        {.kind = core::KeyboardBindingKind::Key, .code = virtualKey}));
}

QString SettingsController::bindingLabel(const QString& binding) const
{
    using namespace Qt::StringLiterals;

    if (binding.size() == 1)
    {
        return binding;
    }

    if (binding == u"Mouse"_s)
    {
        return u"Mouse"_s;
    }

    if (binding == u"MouseLeft"_s)
    {
        return u"Botão esquerdo"_s;
    }

    if (binding == u"MouseRight"_s)
    {
        return u"Botão direito"_s;
    }

    if (binding == u"MouseMiddle"_s)
    {
        return u"Botão do meio"_s;
    }

    if (binding == u"Space"_s)
    {
        return u"Espaço"_s;
    }

    if (binding == u"Shift"_s)
    {
        return u"Shift"_s;
    }

    if (binding == u"Ctrl"_s)
    {
        return u"Ctrl"_s;
    }

    if (binding == u"Alt"_s)
    {
        return u"Alt"_s;
    }

    if (binding == u"Up"_s)
    {
        return u"↑"_s;
    }

    if (binding == u"Down"_s)
    {
        return u"↓"_s;
    }

    if (binding == u"Left"_s)
    {
        return u"←"_s;
    }

    if (binding == u"Right"_s)
    {
        return u"→"_s;
    }

    if (binding == u"Escape"_s)
    {
        return u"Esc"_s;
    }

    if (binding == u"Semicolon"_s)
    {
        return u";"_s;
    }

    if (binding == u"Plus"_s)
    {
        return u"+"_s;
    }

    if (binding == u"Comma"_s)
    {
        return u","_s;
    }

    if (binding == u"Minus"_s)
    {
        return u"-"_s;
    }

    if (binding == u"Period"_s)
    {
        return u"."_s;
    }

    if (binding == u"Slash"_s)
    {
        return u"/"_s;
    }

    if (binding == u"Backtick"_s)
    {
        return u"`"_s;
    }

    if (binding == u"LeftBracket"_s)
    {
        return u"["_s;
    }

    if (binding == u"Backslash"_s)
    {
        return u"\\"_s;
    }

    if (binding == u"RightBracket"_s)
    {
        return u"]"_s;
    }

    if (binding == u"Quote"_s)
    {
        return u"'"_s;
    }

    return binding;
}

void SettingsController::apply(const core::VideoSettings& video)
{
    if (video == settings_.video)
    {
        return;
    }

    settings_.video = video;
    spdlog::info(
        "Video settings: {}p, {} fps, {}, {} Mbps, {}, HDR {}, window {} {}x{}", resolution(),
        video.framesPerSecond, core::codecName(video.codec), bitrateMbps(),
        presentModeName(video.presentMode), core::hdrModeName(video.hdr),
        core::windowModeName(video.windowMode), video.windowSize.width, video.windowSize.height);
    emit videoChanged();
    save();
}

void SettingsController::apply(const core::AudioSettings& audio)
{
    if (audio == settings_.audio)
    {
        return;
    }

    settings_.audio = audio;
    spdlog::info("Audio settings: output '{}', microphone '{}', {}, {}%",
                 audio.outputDevice.empty() ? "Windows default" : audio.outputDevice,
                 audio.microphoneDevice.empty() ? "Windows default" : audio.microphoneDevice,
                 audio.microphoneMuted ? "muted" : "unmuted", audio.volumePercent);
    emit audioChanged();
    save();
}

void SettingsController::apply(const core::ControllerSettings& controllerSettings)
{
    if (controllerSettings == settings_.controller)
    {
        return;
    }

    settings_.controller = controllerSettings;
    spdlog::info("Controller settings: preferred '{}'",
                 controllerSettings.preferred.empty() ? "first connected"
                                                      : controllerSettings.preferred);
    emit controllerChanged();
    save();
}

void SettingsController::applyRtx(const core::RtxSettings& requested)
{
    core::RtxSettings rtx = requested;

    // Reflex is a precondition of Frame Generation, whichever order the two
    // were chosen in.
    if (rtx.frameGeneration == core::FrameGenerationMode::X2 &&
        rtx.reflex == core::ReflexMode::Off)
    {
        rtx.reflex = core::ReflexMode::On;
        spdlog::info("Frame Generation requires Reflex; Reflex turned on");
    }

    if (rtx == settings_.rtx)
    {
        return;
    }

    settings_.rtx = rtx;
    spdlog::info("RTX settings: Reflex {}, upscaling {}, optical flow {}, frame generation {}",
                 render::reflexModeName(rtx.reflex), core::upscalingName(rtx.upscaling),
                 rtx.opticalFlow ? "on" : "off",
                 core::frameGenerationName(rtx.frameGeneration));
    emit rtxChanged();
    save();
}

void SettingsController::applyOverlay(const core::OverlaySettings& overlay)
{
    if (overlay == settings_.overlay)
    {
        return;
    }

    settings_.overlay = overlay;
    spdlog::info("Overlay settings: statistics {}", overlay.statisticsVisible ? "shown" : "hidden");
    emit overlayChanged();
    save();
}

void SettingsController::applyKeyboardMouse(const core::KeyboardMouseSettings& keyboardMouse)
{
    if (keyboardMouse == settings_.keyboardMouse)
    {
        return;
    }

    settings_.keyboardMouse = keyboardMouse;
    spdlog::info("Keyboard and mouse: sensitivity {:.2f}, invert Y {}", keyboardMouse.mouseSensitivity,
                 keyboardMouse.invertY ? "on" : "off");
    emit keyboardMouseChanged();
    save();
}

void SettingsController::save()
{
    if (!store_)
    {
        return;
    }

    if (const auto saved = store_->save(settings_); !saved)
    {
        spdlog::error("Cannot save the settings: {}", saved.error());
        emit notice(u"danger"_s, u"Não foi possível salvar as configurações"_s,
                    u"As mudanças valem até fechar o Lynx Play. Veja o log."_s);
    }
}

} // namespace lynx::app
