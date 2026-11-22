#include "app/SettingsController.h"

#include "audio/Devices.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

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

} // namespace

SettingsController::SettingsController(std::optional<core::SettingsStore> store, QObject* parent)
    : QObject(parent), store_(std::move(store))
{
    if (store_)
    {
        settings_ = store_->load();
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
}

const core::VideoSettings& SettingsController::video() const
{
    return settings_.video;
}

const core::AudioSettings& SettingsController::audio() const
{
    return settings_.audio;
}

const core::ControllerSettings& SettingsController::controller() const
{
    return settings_.controller;
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

bool SettingsController::videoSuperResolution() const
{
    return settings_.rtx.videoSuperResolution;
}

void SettingsController::setVideoSuperResolution(bool enabled)
{
    core::RtxSettings rtx = settings_.rtx;
    rtx.videoSuperResolution = enabled;
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

void SettingsController::apply(const core::VideoSettings& video)
{
    if (video == settings_.video)
    {
        return;
    }

    settings_.video = video;
    spdlog::info("Video settings: {}p, {} fps, {}, {} Mbps, {}", resolution(),
                 video.framesPerSecond, core::codecName(video.codec), bitrateMbps(),
                 presentModeName(video.presentMode));
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

void SettingsController::applyRtx(const core::RtxSettings& rtx)
{
    if (rtx == settings_.rtx)
    {
        return;
    }

    settings_.rtx = rtx;
    spdlog::info("RTX settings: Reflex {}, VSR {}", render::reflexModeName(rtx.reflex),
                 rtx.videoSuperResolution ? "on" : "off");
    emit rtxChanged();
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
