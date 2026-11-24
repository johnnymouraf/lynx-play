#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QtQmlIntegration>

#include <memory>

namespace mfplay::input
{
class GamepadManager;
}

namespace mfplay::app
{

class SettingsController;

// The controllers and audio devices as the interface shows them, polled from
// SDL on the interface thread.
class InputController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main()")

    Q_PROPERTY(bool controllerConnected READ controllerConnected NOTIFY changed)
    Q_PROPERTY(QString controllerTitle READ controllerTitle NOTIFY changed)
    Q_PROPERTY(QString controllerDetail READ controllerDetail NOTIFY changed)
    Q_PROPERTY(QString controllerStatus READ controllerStatus NOTIFY changed)
    Q_PROPERTY(bool controllerWired READ controllerWired NOTIFY changed)
    Q_PROPERTY(bool controllerBatteryLow READ controllerBatteryLow NOTIFY changed)
    Q_PROPERTY(int controllerBattery READ controllerBattery NOTIFY changed)
    Q_PROPERTY(QVariantList controllers READ controllers NOTIFY changed)
    Q_PROPERTY(QString preferredController READ preferredController WRITE setPreferredController
                   NOTIFY changed)

public:
    // `gamepads` may be null when SDL has no gamepad support; audio devices
    // still work.
    InputController(SettingsController& settings,
                    std::unique_ptr<input::GamepadManager> gamepads, bool sdlReady,
                    QObject* parent = nullptr);
    ~InputController() override;

    [[nodiscard]] input::GamepadManager* gamepads() const;

    [[nodiscard]] bool controllerConnected() const;
    [[nodiscard]] QString controllerTitle() const;
    [[nodiscard]] QString controllerDetail() const;
    [[nodiscard]] QString controllerStatus() const;
    [[nodiscard]] bool controllerWired() const;
    [[nodiscard]] bool controllerBatteryLow() const;
    // The active wireless controller's charge in percent, or -1 when it is
    // wired or doesn't report one.
    [[nodiscard]] int controllerBattery() const;
    [[nodiscard]] QVariantList controllers() const;

    [[nodiscard]] QString preferredController() const;
    void setPreferredController(const QString& name);

signals:
    void changed();
    // The DualSense's mute button was pressed once.
    void microphoneButtonPressed();

private:
    void poll();
    void refreshStatus();

    SettingsController& settings_;
    std::unique_ptr<input::GamepadManager> gamepads_;
    bool sdlReady_ = false;
    QTimer timer_;

    int statusTicks_ = 0;
    bool connected_ = false;
    QString title_;
    QString detail_;
    QString status_;
    bool wired_ = false;
    bool batteryLow_ = false;
    int battery_ = -1;
    QVariantList controllers_;
};

} // namespace mfplay::app
