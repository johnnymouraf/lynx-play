#pragma once

#include "core/Settings.h"
#include "render/SwapChain.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration>

#include <optional>

namespace lynx::app
{

// The Configurações › Vídeo options, saved to settings.json on every change.
class SettingsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main()")

    Q_PROPERTY(int resolution READ resolution WRITE setResolution NOTIFY videoChanged)
    Q_PROPERTY(
        int framesPerSecond READ framesPerSecond WRITE setFramesPerSecond NOTIFY videoChanged)
    Q_PROPERTY(QString codec READ codec WRITE setCodec NOTIFY videoChanged)
    Q_PROPERTY(int bitrateMbps READ bitrateMbps WRITE setBitrateMbps NOTIFY videoChanged)
    Q_PROPERTY(bool lowestLatency READ lowestLatency WRITE setLowestLatency NOTIFY videoChanged)
    Q_PROPERTY(int minBitrateMbps READ minBitrateMbps CONSTANT)
    Q_PROPERTY(int maxBitrateMbps READ maxBitrateMbps CONSTANT)
    Q_PROPERTY(QStringList outputDevices READ outputDevices NOTIFY audioDevicesChanged)
    Q_PROPERTY(QStringList microphoneDevices READ microphoneDevices NOTIFY audioDevicesChanged)
    Q_PROPERTY(QString outputDevice READ outputDevice WRITE setOutputDevice NOTIFY audioChanged)
    Q_PROPERTY(
        QString microphoneDevice READ microphoneDevice WRITE setMicrophoneDevice NOTIFY audioChanged)
    Q_PROPERTY(
        bool microphoneMuted READ microphoneMuted WRITE setMicrophoneMuted NOTIFY audioChanged)
    Q_PROPERTY(int volumePercent READ volumePercent WRITE setVolumePercent NOTIFY audioChanged)
    Q_PROPERTY(
        QString preferredController READ preferredController WRITE setPreferredController NOTIFY
            controllerChanged)

public:
    // Without a store, changes last until the app closes.
    explicit SettingsController(std::optional<core::SettingsStore> store,
                                QObject* parent = nullptr);

    [[nodiscard]] const core::VideoSettings& video() const;
    [[nodiscard]] const core::AudioSettings& audio() const;
    [[nodiscard]] const core::ControllerSettings& controller() const;
    [[nodiscard]] render::PresentTiming presentTiming() const;

    // The lines of the resolution asked of the PS5: 360, 540, 720 or 1080.
    [[nodiscard]] int resolution() const;
    void setResolution(int lines);

    // 30 or 60.
    [[nodiscard]] int framesPerSecond() const;
    void setFramesPerSecond(int rate);

    // "h264" or "h265".
    [[nodiscard]] QString codec() const;
    void setCodec(const QString& name);

    [[nodiscard]] int bitrateMbps() const;
    void setBitrateMbps(int megabits);

    [[nodiscard]] bool lowestLatency() const;
    void setLowestLatency(bool lowest);

    [[nodiscard]] int minBitrateMbps() const;
    [[nodiscard]] int maxBitrateMbps() const;

    // Refreshes the device lists after SDL has enumerated them or Windows
    // reported a change.
    void refreshAudioDevices();

    [[nodiscard]] QStringList outputDevices() const;
    [[nodiscard]] QStringList microphoneDevices() const;

    // Empty means the Windows default.
    [[nodiscard]] QString outputDevice() const;
    void setOutputDevice(const QString& name);
    [[nodiscard]] QString microphoneDevice() const;
    void setMicrophoneDevice(const QString& name);

    [[nodiscard]] bool microphoneMuted() const;
    void setMicrophoneMuted(bool muted);

    [[nodiscard]] int volumePercent() const;
    void setVolumePercent(int percent);

    [[nodiscard]] QString preferredController() const;
    void setPreferredController(const QString& name);

signals:
    void videoChanged();
    void audioChanged();
    void audioDevicesChanged();
    void controllerChanged();
    // A message for a toast; `tone` is info, ready, standby or danger.
    void notice(const QString& tone, const QString& title, const QString& message);

private:
    void apply(const core::VideoSettings& video);
    void apply(const core::AudioSettings& audio);
    void apply(const core::ControllerSettings& controller);
    void save();

    std::optional<core::SettingsStore> store_;
    core::Settings settings_;
    QStringList outputDevices_;
    QStringList microphoneDevices_;
    bool devicesLogged_ = false;
};

} // namespace lynx::app
