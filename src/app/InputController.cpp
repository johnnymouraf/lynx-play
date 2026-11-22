#include "app/InputController.h"

#include "app/SettingsController.h"
#include "audio/Devices.h"
#include "input/GamepadManager.h"

#include <QStringList>
#include <QVariantMap>

#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

namespace lynx::app
{

namespace
{

using namespace Qt::StringLiterals;

constexpr int kPollMilliseconds = 4;
// Connection state does not change 250 times a second.
constexpr int kStatusRefreshTicks = 16;
constexpr int kLowBatteryPercent = 20;

QString shortName(const std::string& name)
{
    const QString full = QString::fromStdString(name);

    if (full.contains(u"DualSense Edge"_s, Qt::CaseInsensitive))
    {
        return u"DualSense Edge"_s;
    }

    if (full.contains(u"DualSense"_s, Qt::CaseInsensitive))
    {
        return u"DualSense"_s;
    }

    return full;
}

QString connectionText(input::GamepadConnection connection)
{
    switch (connection)
    {
    case input::GamepadConnection::Wired:
        return u"Conectado por USB"_s;
    case input::GamepadConnection::Wireless:
        return u"Conectado por Bluetooth"_s;
    case input::GamepadConnection::Unknown:
        break;
    }

    return u"Conectado"_s;
}

QString connectionShort(input::GamepadConnection connection)
{
    switch (connection)
    {
    case input::GamepadConnection::Wired:
        return u"USB"_s;
    case input::GamepadConnection::Wireless:
        return u"Bluetooth"_s;
    case input::GamepadConnection::Unknown:
        break;
    }

    return u"conexão desconhecida"_s;
}

} // namespace

InputController::InputController(SettingsController& settings,
                                 std::unique_ptr<input::GamepadManager> gamepads, bool sdlReady,
                                 QObject* parent)
    : QObject(parent), settings_(settings), gamepads_(std::move(gamepads)), sdlReady_(sdlReady)
{
    settings_.refreshAudioDevices();

    timer_.setInterval(kPollMilliseconds);
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &InputController::poll);
    timer_.start();

    connect(&settings_, &SettingsController::controllerChanged, this,
            [this]
            {
                if (gamepads_ != nullptr)
                {
                    gamepads_->setPreferred(settings_.preferredController().toStdString());
                }

                refreshStatus();
            });

    refreshStatus();
}

InputController::~InputController() = default;

input::GamepadManager* InputController::gamepads() const
{
    return gamepads_.get();
}

bool InputController::controllerConnected() const
{
    return connected_;
}

QString InputController::controllerTitle() const
{
    return title_;
}

QString InputController::controllerDetail() const
{
    return detail_;
}

QString InputController::controllerStatus() const
{
    return status_;
}

bool InputController::controllerWired() const
{
    return wired_;
}

bool InputController::controllerBatteryLow() const
{
    return batteryLow_;
}

QVariantList InputController::controllers() const
{
    return controllers_;
}

QString InputController::preferredController() const
{
    return settings_.preferredController();
}

void InputController::setPreferredController(const QString& name)
{
    if (preferredController() == name)
    {
        return;
    }

    const std::string preferred =
        name.isEmpty() ? std::string("the first connected") : name.toStdString();
    spdlog::info("Preferred controller: {}", preferred);
    settings_.setPreferredController(name);

    if (gamepads_ != nullptr)
    {
        gamepads_->setPreferred(name.toStdString());
    }

    refreshStatus();
    emit changed();
}

void InputController::poll()
{
    if (sdlReady_)
    {
        SDL_PumpEvents();
        bool audioDevicesChanged = false;
        SDL_Event event;

        while (SDL_PollEvent(&event))
        {
            if (gamepads_ != nullptr)
            {
                gamepads_->handleEvent(event);
            }

            switch (event.type)
            {
            case SDL_EVENT_AUDIO_DEVICE_ADDED:
                audio::dispatchAudioDeviceEvent(true, event.adevice.recording,
                                                event.adevice.which);
                audioDevicesChanged = true;
                break;
            case SDL_EVENT_AUDIO_DEVICE_REMOVED:
                audio::dispatchAudioDeviceEvent(false, event.adevice.recording,
                                                event.adevice.which);
                audioDevicesChanged = true;
                break;
            default:
                break;
            }
        }

        if (gamepads_ != nullptr)
        {
            gamepads_->tick();

            if (gamepads_->consumeMicrophoneButton())
            {
                emit microphoneButtonPressed();
            }
        }

        if (audioDevicesChanged)
        {
            settings_.refreshAudioDevices();
        }
    }

    if (++statusTicks_ >= kStatusRefreshTicks)
    {
        statusTicks_ = 0;
        refreshStatus();
    }
}

void InputController::refreshStatus()
{
    const std::vector<input::GamepadInfo> gamepads =
        gamepads_ != nullptr ? gamepads_->gamepads() : std::vector<input::GamepadInfo>{};

    QVariantList controllers;
    controllers.reserve(static_cast<qsizetype>(gamepads.size()));
    const input::GamepadInfo* active = nullptr;

    for (const input::GamepadInfo& gamepad : gamepads)
    {
        if (gamepad.active)
        {
            active = &gamepad;
        }

        QVariantMap entry;
        entry.insert(u"name"_s, QString::fromStdString(gamepad.name));
        entry.insert(u"label"_s, shortName(gamepad.name) + u" ("_s +
                                     connectionShort(gamepad.connection) + u")"_s);
        entry.insert(u"active"_s, gamepad.active);
        controllers.push_back(entry);
    }

    const bool connected = active != nullptr;
    QString title;
    QString detail;
    QString status;
    bool wired = false;
    bool batteryLow = false;

    if (active != nullptr)
    {
        const QString name = shortName(active->name);

        if (gamepads.size() > 1)
        {
            title = QStringLiteral("%1 controles").arg(static_cast<int>(gamepads.size()));
            detail = name + u" · "_s + connectionShort(active->connection);
        }
        else
        {
            title = name;
            detail = connectionText(active->connection);
        }

        wired = active->connection == input::GamepadConnection::Wired;

        if (wired)
        {
            status = u"USB"_s;
        }
        else if (active->batteryPercent >= 0)
        {
            status = QStringLiteral("Bateria %1 %").arg(active->batteryPercent);
            batteryLow = active->batteryPercent < kLowBatteryPercent;
        }
        else
        {
            status = connectionShort(active->connection);
        }
    }

    const bool statusChanged =
        connected != connected_ || title != title_ || detail != detail_ || status != status_ ||
        wired != wired_ || batteryLow != batteryLow_ || controllers != controllers_;

    connected_ = connected;
    title_ = title;
    detail_ = detail;
    status_ = status;
    wired_ = wired;
    batteryLow_ = batteryLow;
    controllers_ = controllers;

    if (statusChanged)
    {
        emit changed();
    }
}

} // namespace lynx::app
