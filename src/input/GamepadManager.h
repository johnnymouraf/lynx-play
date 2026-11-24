#pragma once

#include "input/ControllerMapping.h"
#include "input/DualSenseEffects.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL_events.h>

namespace mfplay::input
{

enum class GamepadConnection
{
    Unknown,
    Wired,
    Wireless,
};

struct GamepadInfo
{
    std::string name;
    bool dualSense = false;
    GamepadConnection connection = GamepadConnection::Unknown;
    int batteryPercent = -1;
    bool active = false;
};

// The gamepads SDL sees. One is active (the preferred one, else the first);
// its sample goes to the console, and console feedback is sent to it. SDL
// event pumping and tick() run on the main thread; sample() and submit() are
// safe from other threads.
class GamepadManager
{
public:
    [[nodiscard]] static std::expected<std::unique_ptr<GamepadManager>, std::string> create(
        std::string preferredName);

    ~GamepadManager();

    GamepadManager(const GamepadManager&) = delete;
    GamepadManager& operator=(const GamepadManager&) = delete;
    GamepadManager(GamepadManager&&) = delete;
    GamepadManager& operator=(GamepadManager&&) = delete;

    // One SDL event from the app's pump.
    void handleEvent(const SDL_Event& event);
    // Applies queued feedback and publishes the active controller's state.
    void tick();

    [[nodiscard]] std::vector<GamepadInfo> gamepads() const;
    // Every connected controller, combined like chiaki's controller state OR.
    [[nodiscard]] std::optional<ChiakiControllerState> gamepadState() const;
    [[nodiscard]] std::optional<GamepadConnection> connection() const;
    [[nodiscard]] std::optional<std::string> activeName() const;

    // While the overlay's menu is open, its input drives the menu and the
    // console sees the controller at rest.
    void setOverlayOpen(bool open);
    [[nodiscard]] bool overlayOpen() const;

    // The microphone button on the DualSense, as a one-shot edge.
    [[nodiscard]] bool consumeMicrophoneButton();

    // Changes which controller is active; the first one with this exact name
    // wins.
    void setPreferred(const std::string& name);

    // Console feedback, copied into a queue applied on the next tick.
    void submit(const DualSenseCommand& command);
    void resetMotion();

private:
    struct Device;

    explicit GamepadManager(std::string preferredName);

    bool openAll();
    void openDevice(SDL_JoystickID id);
    void closeDevice(SDL_JoystickID id);
    void refreshActive();
    void refreshDevices();
    void publish();
    void applyCommand(Device& device, const DualSenseCommand& command);
    void sendEffects(Device& device);
    void sendRumble(Device& device, const std::pair<std::uint16_t, std::uint16_t>& levels);
    void resetDeviceMotion(Device& device);
    void readCurrentState(Device& device);
    void updateMotion(Device& device, bool accelerometer, const std::array<float, 3>& data,
                      std::uint32_t timestamp);
    [[nodiscard]] Device* findDevice(SDL_JoystickID id);
    [[nodiscard]] Device* activeDevice();

    std::string preferred_;
    std::vector<std::unique_ptr<Device>> devices_;
    std::optional<SDL_JoystickID> activeId_;
    DualSenseEffects effects_;
    // The console's intensity byte; 0 is strong on both axes, as chiaki-ng
    // starts.
    std::uint8_t intensity_ = 0;
    // While the DualSense's USB audio endpoint drives the actuators, SDL's
    // rumble must stay off; the effect packet carries rumble instead.
    bool audioHapticsActive_ = false;
    bool effectsApplied_ = false;
    // The last lightbar and player LED the console asked for, so a controller
    // that appears later gets them too.
    std::optional<std::array<std::uint8_t, 3>> led_;
    std::optional<int> playerIndex_;
    bool packetErrorLogged_ = false;
    bool rumbleErrorLogged_ = false;
    std::chrono::steady_clock::time_point nextRefresh_{};
    std::atomic<bool> microphonePressed_{false};
    std::atomic<bool> overlayOpen_{false};

    mutable std::mutex mutex_;
    std::vector<GamepadInfo> infos_;
    std::optional<ChiakiControllerState> activeState_;
    std::optional<GamepadConnection> activeConnection_;
    std::optional<std::string> activeName_;

    std::mutex commandMutex_;
    std::vector<DualSenseCommand> commands_;
    bool resetMotionRequested_ = false;
};

} // namespace mfplay::input
