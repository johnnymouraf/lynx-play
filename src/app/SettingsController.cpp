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

    reflexAvailability_ = render::ReflexSession::probe();

    if (reflexAvailability_.available)
    {
        spdlog::info("Reflex is available ({})", reflexAvailability_.detail);
    }
    else
    {
        spdlog::info("Reflex is unavailable: {}", reflexAvailability_.detail);
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
    spdlog::info("RTX settings: Reflex {}", render::reflexModeName(rtx.reflex));
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
