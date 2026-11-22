#include "input/GamepadManager.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <utility>

#include <SDL3/SDL.h>
#include <chiaki/orientation.h>
#include <spdlog/spdlog.h>

namespace lynx::input
{

namespace
{

constexpr std::uint16_t kSonyVendor = 0x054C;
constexpr std::uint16_t kDualSenseProduct = 0x0CE6;
constexpr std::uint16_t kDualSenseEdgeProduct = 0x0DF2;
constexpr std::uint32_t kRumbleDurationMs = 5000;
constexpr auto kDeviceRefreshInterval = std::chrono::milliseconds(500);

struct ButtonEntry
{
    SDL_GamepadButton sdl;
    std::uint32_t mask;
};

constexpr std::array<ButtonEntry, 15> kButtons{{
    {SDL_GAMEPAD_BUTTON_SOUTH, buttons::kCross},
    {SDL_GAMEPAD_BUTTON_EAST, buttons::kCircle},
    {SDL_GAMEPAD_BUTTON_WEST, buttons::kSquare},
    {SDL_GAMEPAD_BUTTON_NORTH, buttons::kTriangle},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, buttons::kDpadLeft},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, buttons::kDpadRight},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, buttons::kDpadUp},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, buttons::kDpadDown},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, buttons::kL1},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, buttons::kR1},
    {SDL_GAMEPAD_BUTTON_LEFT_STICK, buttons::kL3},
    {SDL_GAMEPAD_BUTTON_RIGHT_STICK, buttons::kR3},
    {SDL_GAMEPAD_BUTTON_START, buttons::kOptions},
    {SDL_GAMEPAD_BUTTON_BACK, buttons::kCreate},
    {SDL_GAMEPAD_BUTTON_TOUCHPAD, buttons::kTouchpad},
}};

std::uint32_t buttonMask(SDL_GamepadButton button)
{
    const auto found = std::ranges::find(kButtons, button, &ButtonEntry::sdl);
    return found != kButtons.end() ? found->mask : 0;
}

GamepadConnection connectionOf(SDL_JoystickConnectionState state)
{
    switch (state)
    {
    case SDL_JOYSTICK_CONNECTION_WIRED:
        return GamepadConnection::Wired;
    case SDL_JOYSTICK_CONNECTION_WIRELESS:
        return GamepadConnection::Wireless;
    case SDL_JOYSTICK_CONNECTION_INVALID:
    case SDL_JOYSTICK_CONNECTION_UNKNOWN:
        break;
    }

    return GamepadConnection::Unknown;
}

const char* connectionName(GamepadConnection connection)
{
    switch (connection)
    {
    case GamepadConnection::Wired:
        return "USB";
    case GamepadConnection::Wireless:
        return "Bluetooth";
    case GamepadConnection::Unknown:
        break;
    }

    return "unknown connection";
}

int batteryPercentOf(SDL_Gamepad* gamepad)
{
    int percent = -1;
    const SDL_PowerState state = SDL_GetGamepadPowerInfo(gamepad, &percent);

    if (state == SDL_POWERSTATE_ERROR || state == SDL_POWERSTATE_UNKNOWN || percent < 0)
    {
        return -1;
    }

    return percent;
}

void clearDualSenseEffects(SDL_Gamepad* gamepad)
{
    DualSenseEffects effects;
    DualSenseTriggerEffect clear;
    clear.type = 0x05;
    effects.apply({.leftTrigger = clear, .rightTrigger = clear});
    const auto packet = effects.serialize();
    SDL_SendGamepadEffect(gamepad, packet.data(), static_cast<int>(packet.size()));
    SDL_RumbleGamepad(gamepad, 0, 0, 0);
}

} // namespace

struct GamepadManager::Device
{
    SDL_JoystickID id = 0;
    SDL_Gamepad* handle = nullptr;
    std::string name;
    bool dualSense = false;
    bool hasLed = false;
    bool hasPlayerLed = false;
    GamepadConnection connection = GamepadConnection::Unknown;
    int batteryPercent = -1;
    bool microphoneDown = false;

    GamepadSample sample;
    ChiakiOrientationTracker tracker;
    ChiakiAccelNewZero accelZero;
    ChiakiAccelNewZero realAccel;
    std::uint32_t lastMotionMicroseconds = 0;

    Device()
    {
        chiaki_orientation_tracker_init(&tracker);
        chiaki_accel_new_zero_set_inactive(&accelZero, false);
        chiaki_accel_new_zero_set_inactive(&realAccel, true);
    }
};

std::expected<std::unique_ptr<GamepadManager>, std::string> GamepadManager::create(
    std::string preferredName)
{
    if (SDL_WasInit(SDL_INIT_GAMEPAD) == 0)
    {
        return std::unexpected("SDL gamepads are not initialized");
    }

    auto manager = std::unique_ptr<GamepadManager>(new GamepadManager(std::move(preferredName)));

    if (!manager->openAll())
    {
        return std::unexpected("SDL cannot list gamepads");
    }

    manager->refreshActive();
    return manager;
}

GamepadManager::GamepadManager(std::string preferredName) : preferred_(std::move(preferredName))
{
}

GamepadManager::~GamepadManager()
{
    for (const std::unique_ptr<Device>& device : devices_)
    {
        if (device->dualSense)
        {
            clearDualSenseEffects(device->handle);
        }

        SDL_CloseGamepad(device->handle);
    }
}

bool GamepadManager::openAll()
{
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);

    if (ids == nullptr)
    {
        return false;
    }

    spdlog::info("SDL reports {} gamepad(s)", count);

    for (int i = 0; i < count; ++i)
    {
        openDevice(ids[i]);
    }

    SDL_free(ids);
    return true;
}

void GamepadManager::openDevice(SDL_JoystickID id)
{
    if (findDevice(id) != nullptr)
    {
        return;
    }

    SDL_Gamepad* handle = SDL_OpenGamepad(id);

    if (handle == nullptr)
    {
        spdlog::warn("Cannot open gamepad {}: {}", id, SDL_GetError());
        return;
    }

    auto device = std::make_unique<Device>();
    device->id = id;
    device->handle = handle;

    const char* name = SDL_GetGamepadName(handle);
    device->name = name != nullptr ? name : "Gamepad";

    const std::uint16_t vendor = SDL_GetGamepadVendor(handle);
    const std::uint16_t product = SDL_GetGamepadProduct(handle);
    const bool sony = vendor == kSonyVendor &&
                      (product == kDualSenseProduct || product == kDualSenseEdgeProduct);
    device->dualSense = SDL_GetGamepadType(handle) == SDL_GAMEPAD_TYPE_PS5 || sony;

    const SDL_PropertiesID properties = SDL_GetGamepadProperties(handle);
    device->hasLed =
        SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, false);
    device->hasPlayerLed =
        SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN, false);

    device->connection = connectionOf(SDL_GetGamepadConnectionState(handle));
    device->batteryPercent = batteryPercentOf(handle);

    if (SDL_GamepadHasSensor(handle, SDL_SENSOR_GYRO))
    {
        SDL_SetGamepadSensorEnabled(handle, SDL_SENSOR_GYRO, true);
        spdlog::info("Gyro enabled at {:.0f} Hz",
                     SDL_GetGamepadSensorDataRate(handle, SDL_SENSOR_GYRO));
    }

    if (SDL_GamepadHasSensor(handle, SDL_SENSOR_ACCEL))
    {
        SDL_SetGamepadSensorEnabled(handle, SDL_SENSOR_ACCEL, true);
        spdlog::info("Accelerometer enabled at {:.0f} Hz",
                     SDL_GetGamepadSensorDataRate(handle, SDL_SENSOR_ACCEL));
    }

    readCurrentState(*device);

    if (device->batteryPercent >= 0)
    {
        spdlog::info("Gamepad detected: '{}' ({}{}, battery {}%)", device->name,
                     device->dualSense ? "DualSense, " : "", connectionName(device->connection),
                     device->batteryPercent);
    }
    else
    {
        spdlog::info("Gamepad detected: '{}' ({}{})", device->name,
                     device->dualSense ? "DualSense, " : "", connectionName(device->connection));
    }

    devices_.push_back(std::move(device));
}

void GamepadManager::closeDevice(SDL_JoystickID id)
{
    const auto found = std::ranges::find(
        devices_, id, [](const std::unique_ptr<Device>& device) { return device->id; });

    if (found == devices_.end())
    {
        return;
    }

    Device& device = **found;
    spdlog::info("Gamepad disconnected: '{}'", device.name);

    if (device.dualSense)
    {
        clearDualSenseEffects(device.handle);
    }

    SDL_CloseGamepad(device.handle);
    devices_.erase(found);

    if (activeId_ && *activeId_ == id)
    {
        activeId_.reset();
    }
}

void GamepadManager::refreshActive()
{
    Device* chosen = nullptr;

    if (!preferred_.empty())
    {
        const auto found = std::ranges::find(
            devices_, preferred_,
            [](const std::unique_ptr<Device>& device) -> const std::string&
            { return device->name; });

        if (found != devices_.end())
        {
            chosen = found->get();
        }
    }

    if (chosen == nullptr && !devices_.empty())
    {
        chosen = devices_.front().get();
    }

    const std::optional<SDL_JoystickID> next = chosen != nullptr ? std::optional(chosen->id)
                                                                 : std::nullopt;

    if (next != activeId_)
    {
        activeId_ = next;
        effects_ = DualSenseEffects{};

        if (chosen != nullptr)
        {
            spdlog::info("Active controller: '{}' ({})", chosen->name,
                         connectionName(chosen->connection));
        }
    }

    publish();
}

void GamepadManager::refreshDevices()
{
    for (const std::unique_ptr<Device>& device : devices_)
    {
        const GamepadConnection connection =
            connectionOf(SDL_GetGamepadConnectionState(device->handle));

        if (connection != device->connection)
        {
            device->connection = connection;
            spdlog::info("'{}' is now on {}", device->name, connectionName(connection));
        }

        device->batteryPercent = batteryPercentOf(device->handle);
    }
}

void GamepadManager::publish()
{
    std::vector<GamepadInfo> infos;
    infos.reserve(devices_.size());

    for (const std::unique_ptr<Device>& device : devices_)
    {
        infos.push_back({
            .name = device->name,
            .dualSense = device->dualSense,
            .connection = device->connection,
            .batteryPercent = device->batteryPercent,
            .active = activeId_ && *activeId_ == device->id,
        });
    }

    std::optional<GamepadSample> sample;
    std::optional<GamepadConnection> connection;
    std::optional<std::string> name;

    if (const Device* device = activeDevice(); device != nullptr)
    {
        sample = device->sample;
        connection = device->connection;
        name = device->name;
    }

    const std::scoped_lock lock(mutex_);
    infos_ = std::move(infos);
    activeSample_ = sample;
    activeConnection_ = connection;
    activeName_ = std::move(name);
}

std::vector<GamepadInfo> GamepadManager::gamepads() const
{
    const std::scoped_lock lock(mutex_);
    return infos_;
}

std::optional<GamepadSample> GamepadManager::sample() const
{
    const std::scoped_lock lock(mutex_);
    return activeSample_;
}

std::optional<GamepadConnection> GamepadManager::connection() const
{
    const std::scoped_lock lock(mutex_);
    return activeConnection_;
}

std::optional<std::string> GamepadManager::activeName() const
{
    const std::scoped_lock lock(mutex_);
    return activeName_;
}

void GamepadManager::setOverlayOpen(bool open)
{
    overlayOpen_.store(open);
}

bool GamepadManager::overlayOpen() const
{
    return overlayOpen_.load();
}

bool GamepadManager::consumeMicrophoneButton()
{
    return microphonePressed_.exchange(false);
}

void GamepadManager::setPreferred(const std::string& name)
{
    preferred_ = name;
    refreshActive();
}

void GamepadManager::submit(const DualSenseCommand& command)
{
    const std::scoped_lock lock(commandMutex_);
    commands_.push_back(command);
}

void GamepadManager::resetMotion()
{
    const std::scoped_lock lock(commandMutex_);
    resetMotionRequested_ = true;
}

void GamepadManager::handleEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_GAMEPAD_ADDED:
        openDevice(event.gdevice.which);
        refreshActive();
        return;

    case SDL_EVENT_GAMEPAD_REMOVED:
        closeDevice(event.gdevice.which);
        refreshActive();
        return;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    {
        Device* device = findDevice(event.gbutton.which);

        if (device == nullptr)
        {
            return;
        }

        const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
        const auto button = static_cast<SDL_GamepadButton>(event.gbutton.button);

        if (button == SDL_GAMEPAD_BUTTON_MISC1)
        {
            if (down && !device->microphoneDown)
            {
                microphonePressed_.store(true);
            }

            device->microphoneDown = down;
            return;
        }

        device->sample.buttons = down ? device->sample.buttons | buttonMask(button)
                                      : device->sample.buttons & ~buttonMask(button);
        return;
    }

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
    {
        Device* device = findDevice(event.gaxis.which);

        if (device == nullptr)
        {
            return;
        }

        const auto axis = static_cast<SDL_GamepadAxis>(event.gaxis.axis);

        switch (axis)
        {
        case SDL_GAMEPAD_AXIS_LEFTX:
            device->sample.leftX = event.gaxis.value;
            return;
        case SDL_GAMEPAD_AXIS_LEFTY:
            device->sample.leftY = event.gaxis.value;
            return;
        case SDL_GAMEPAD_AXIS_RIGHTX:
            device->sample.rightX = event.gaxis.value;
            return;
        case SDL_GAMEPAD_AXIS_RIGHTY:
            device->sample.rightY = event.gaxis.value;
            return;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
            device->sample.leftTrigger = triggerLevel(event.gaxis.value);
            return;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
            device->sample.rightTrigger = triggerLevel(event.gaxis.value);
            return;
        case SDL_GAMEPAD_AXIS_INVALID:
        case SDL_GAMEPAD_AXIS_COUNT:
            return;
        }

        return;
    }

    case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
    {
        Device* device = findDevice(event.gtouchpad.which);

        if (device == nullptr || event.gtouchpad.touchpad != 0)
        {
            return;
        }

        const int finger = event.gtouchpad.finger;

        if (finger < 0 || finger >= static_cast<int>(device->sample.touches.size()))
        {
            return;
        }

        GamepadTouch& touch = device->sample.touches[static_cast<std::size_t>(finger)];
        touch.x = event.gtouchpad.x;
        touch.y = event.gtouchpad.y;
        touch.down = event.type != SDL_EVENT_GAMEPAD_TOUCHPAD_UP;
        return;
    }

    case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
    {
        Device* device = findDevice(event.gsensor.which);

        if (device == nullptr)
        {
            return;
        }

        const auto timestamp = static_cast<std::uint32_t>(event.gsensor.sensor_timestamp / 1000);

        if (event.gsensor.sensor == SDL_SENSOR_ACCEL)
        {
            device->sample.accel = {
                event.gsensor.data[0] / SDL_STANDARD_GRAVITY,
                event.gsensor.data[1] / SDL_STANDARD_GRAVITY,
                event.gsensor.data[2] / SDL_STANDARD_GRAVITY,
            };
            chiaki_accel_new_zero_set_active(&device->realAccel, device->sample.accel[0],
                                             device->sample.accel[1], device->sample.accel[2],
                                             true);
            chiaki_orientation_tracker_update(
                &device->tracker, device->sample.gyro[0], device->sample.gyro[1],
                device->sample.gyro[2], device->sample.accel[0], device->sample.accel[1],
                device->sample.accel[2], &device->accelZero, false, timestamp);
        }
        else if (event.gsensor.sensor == SDL_SENSOR_GYRO)
        {
            device->sample.gyro = {
                event.gsensor.data[0],
                event.gsensor.data[1],
                event.gsensor.data[2],
            };
            chiaki_orientation_tracker_update(
                &device->tracker, device->sample.gyro[0], device->sample.gyro[1],
                device->sample.gyro[2], device->sample.accel[0], device->sample.accel[1],
                device->sample.accel[2], &device->accelZero, false, timestamp);
        }
        else
        {
            return;
        }

        device->lastMotionMicroseconds = timestamp;
        device->sample.hasMotion = true;

        ChiakiControllerState state;
        chiaki_controller_state_set_idle(&state);
        chiaki_orientation_tracker_apply_to_controller_state(&device->tracker, &state);
        device->sample.orientation = {
            state.orient_x,
            state.orient_y,
            state.orient_z,
            state.orient_w,
        };
        return;
    }

    default:
        return;
    }
}

void GamepadManager::tick()
{
    std::vector<DualSenseCommand> commands;
    bool resetMotion = false;

    {
        const std::scoped_lock lock(commandMutex_);
        commands.swap(commands_);
        resetMotion = resetMotionRequested_;
        resetMotionRequested_ = false;
    }

    if (Device* device = activeDevice(); device != nullptr)
    {
        if (resetMotion)
        {
            resetDeviceMotion(*device);
        }

        for (const DualSenseCommand& command : commands)
        {
            applyCommand(*device, command);
        }
    }

    const auto now = std::chrono::steady_clock::now();

    if (now >= nextRefresh_)
    {
        refreshDevices();
        nextRefresh_ = now + kDeviceRefreshInterval;
    }

    publish();
}

void GamepadManager::applyCommand(Device& device, const DualSenseCommand& command)
{
    if (command.rumble)
    {
        if (!SDL_RumbleGamepad(device.handle, command.rumble->first, command.rumble->second,
                                kRumbleDurationMs) &&
            !rumbleErrorLogged_)
        {
            spdlog::warn("Cannot rumble '{}': {}", device.name, SDL_GetError());
            rumbleErrorLogged_ = true;
        }
    }

    if (command.led && device.hasLed)
    {
        SDL_SetGamepadLED(device.handle, (*command.led)[0], (*command.led)[1], (*command.led)[2]);
    }

    if (command.playerIndex && device.hasPlayerLed)
    {
        SDL_SetGamepadPlayerIndex(device.handle, *command.playerIndex);
    }

    const bool packet = command.leftTrigger || command.rightTrigger || command.microphoneMuted ||
                        command.intensity || command.classicRumble;

    if (!packet)
    {
        return;
    }

    effects_.apply(command);

    if (device.dualSense)
    {
        sendEffects(device);
    }
}

void GamepadManager::sendEffects(Device& device)
{
    const auto packet = effects_.serialize();

    if (!SDL_SendGamepadEffect(device.handle, packet.data(), static_cast<int>(packet.size())) &&
        !packetErrorLogged_)
    {
        spdlog::warn("Cannot send DualSense effects: {}", SDL_GetError());
        packetErrorLogged_ = true;
    }
}

void GamepadManager::resetDeviceMotion(Device& device)
{
    chiaki_accel_new_zero_set_active(&device.accelZero, device.realAccel.accel_x,
                                     device.realAccel.accel_y, device.realAccel.accel_z, false);
    chiaki_orientation_tracker_init(&device.tracker);
    chiaki_orientation_tracker_update(
        &device.tracker, device.sample.gyro[0], device.sample.gyro[1], device.sample.gyro[2],
        device.realAccel.accel_x, device.realAccel.accel_y, device.realAccel.accel_z,
        &device.accelZero, false, device.lastMotionMicroseconds);

    ChiakiControllerState state;
    chiaki_controller_state_set_idle(&state);
    chiaki_orientation_tracker_apply_to_controller_state(&device.tracker, &state);
    device.sample.orientation = {
        state.orient_x,
        state.orient_y,
        state.orient_z,
        state.orient_w,
    };
}

void GamepadManager::readCurrentState(Device& device)
{
    device.sample = GamepadSample{};

    for (const ButtonEntry& entry : kButtons)
    {
        if (SDL_GetGamepadButton(device.handle, entry.sdl))
        {
            device.sample.buttons |= entry.mask;
        }
    }

    device.microphoneDown = SDL_GetGamepadButton(device.handle, SDL_GAMEPAD_BUTTON_MISC1);
    device.sample.leftX = SDL_GetGamepadAxis(device.handle, SDL_GAMEPAD_AXIS_LEFTX);
    device.sample.leftY = SDL_GetGamepadAxis(device.handle, SDL_GAMEPAD_AXIS_LEFTY);
    device.sample.rightX = SDL_GetGamepadAxis(device.handle, SDL_GAMEPAD_AXIS_RIGHTX);
    device.sample.rightY = SDL_GetGamepadAxis(device.handle, SDL_GAMEPAD_AXIS_RIGHTY);
    device.sample.leftTrigger =
        triggerLevel(SDL_GetGamepadAxis(device.handle, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    device.sample.rightTrigger =
        triggerLevel(SDL_GetGamepadAxis(device.handle, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));

    const int fingers = SDL_GetNumGamepadTouchpadFingers(device.handle, 0);

    for (int finger = 0; finger < fingers && finger < static_cast<int>(device.sample.touches.size());
         ++finger)
    {
        bool down = false;
        float x = 0.0f;
        float y = 0.0f;
        float pressure = 0.0f;

        if (!SDL_GetGamepadTouchpadFinger(device.handle, 0, finger, &down, &x, &y, &pressure))
        {
            continue;
        }

        GamepadTouch& touch = device.sample.touches[static_cast<std::size_t>(finger)];
        touch.down = down;
        touch.x = x;
        touch.y = y;
    }

    std::array<float, 3> data{};

    if (SDL_GetGamepadSensorData(device.handle, SDL_SENSOR_GYRO, data.data(), 3))
    {
        device.sample.gyro = data;
        device.sample.hasMotion = true;
    }

    if (SDL_GetGamepadSensorData(device.handle, SDL_SENSOR_ACCEL, data.data(), 3))
    {
        device.sample.accel = {
            data[0] / SDL_STANDARD_GRAVITY,
            data[1] / SDL_STANDARD_GRAVITY,
            data[2] / SDL_STANDARD_GRAVITY,
        };
        chiaki_accel_new_zero_set_active(&device.realAccel, device.sample.accel[0],
                                         device.sample.accel[1], device.sample.accel[2], true);
        device.sample.hasMotion = true;
    }
}

GamepadManager::Device* GamepadManager::findDevice(SDL_JoystickID id)
{
    const auto found = std::ranges::find(
        devices_, id, [](const std::unique_ptr<Device>& device) { return device->id; });
    return found != devices_.end() ? found->get() : nullptr;
}

GamepadManager::Device* GamepadManager::activeDevice()
{
    if (!activeId_)
    {
        return nullptr;
    }

    return findDevice(*activeId_);
}

} // namespace lynx::input
