#pragma once

#include <chiaki/controller.h>

#include <SDL3/SDL_gamepad.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace lynx::input
{

// One bit per control, in the same positions SDL reports them; the names are
// the PlayStation ones the console sees.
namespace buttons
{

inline constexpr std::uint32_t kCross = 1u << 0;
inline constexpr std::uint32_t kCircle = 1u << 1;
inline constexpr std::uint32_t kSquare = 1u << 2;
inline constexpr std::uint32_t kTriangle = 1u << 3;
inline constexpr std::uint32_t kDpadLeft = 1u << 4;
inline constexpr std::uint32_t kDpadRight = 1u << 5;
inline constexpr std::uint32_t kDpadUp = 1u << 6;
inline constexpr std::uint32_t kDpadDown = 1u << 7;
inline constexpr std::uint32_t kL1 = 1u << 8;
inline constexpr std::uint32_t kR1 = 1u << 9;
inline constexpr std::uint32_t kL3 = 1u << 10;
inline constexpr std::uint32_t kR3 = 1u << 11;
inline constexpr std::uint32_t kOptions = 1u << 12;
inline constexpr std::uint32_t kCreate = 1u << 13;
inline constexpr std::uint32_t kTouchpad = 1u << 14;
inline constexpr std::uint32_t kPs = 1u << 15;

} // namespace buttons

struct GamepadTouch
{
    bool down = false;
    // Normalized 0..1, origin at the top left, as SDL reports it.
    float x = 0.0f;
    float y = 0.0f;
    // The id the console sees; -1 while the finger is up.
    std::int8_t id = -1;
};

// Everything the console needs to know about the controller at one moment.
struct GamepadSample
{
    std::uint32_t buttons = 0;
    std::uint8_t leftTrigger = 0;
    std::uint8_t rightTrigger = 0;
    std::int16_t leftX = 0;
    std::int16_t leftY = 0;
    std::int16_t rightX = 0;
    std::int16_t rightY = 0;
    // The console allocates a fresh id per touch, masked like chiaki's.
    std::uint8_t touchIdNext = 0;
    std::array<GamepadTouch, CHIAKI_CONTROLLER_TOUCHES_MAX> touches{};
    // Radians per second.
    std::array<float, 3> gyro{};
    // In g, gravity included; a controller at rest reads 1 on Y, as chiaki's
    // idle state does.
    std::array<float, 3> accel{0.0f, 1.0f, 0.0f};
    // The console reads a quaternion.
    std::array<float, 4> orientation{0.0f, 0.0f, 0.0f, 1.0f};
};

// The PS5's touchpad coordinates, as chiaki-ng sends them.
inline constexpr std::uint16_t kTouchpadWidth = 1920;
inline constexpr std::uint16_t kTouchpadHeight = 1079;

namespace touch_id
{

inline constexpr std::uint8_t kMask = 0x7F;

} // namespace touch_id

// SDL3 reports a trigger at rest as 0 and fully pressed as 32767; the console
// takes 0..255, as chiaki-ng's `value >> 7`.
inline constexpr std::int16_t kTriggerMaximum = 32767;

[[nodiscard]] constexpr std::uint8_t triggerLevel(std::int16_t value)
{
    const std::int16_t pressed = std::clamp(value, std::int16_t{0}, kTriggerMaximum);
    return static_cast<std::uint8_t>(pressed >> 7);
}

// SDL reports accelerometer data in m/s²; the console takes g.
inline constexpr float kStandardGravity = 9.80665f;

[[nodiscard]] constexpr std::array<float, 3> accelerationInG(std::array<float, 3> si)
{
    return {
        si[0] / kStandardGravity,
        si[1] / kStandardGravity,
        si[2] / kStandardGravity,
    };
}

[[nodiscard]] std::uint16_t touchCoordinate(float normalized, std::uint16_t maximum);

enum class TouchPhase
{
    Down,
    Motion,
    Up,
};

// Tracks one finger through SDL's phases, allocating the console's touch id
// when it goes down, as chiaki-ng does. `finger` is SDL's finger index.
void applyTouch(GamepadSample& sample, int finger, TouchPhase phase, float x, float y);

// The internal mask for one SDL gamepad button; 0 for the controls the
// console doesn't take as buttons (the microphone is a mute toggle).
[[nodiscard]] std::uint32_t gamepadButtonMask(SDL_GamepadButton button);

// The pressed controls, by chiaki's names, for the debug view.
[[nodiscard]] std::string buttonNames(std::uint32_t chiakiButtons);

[[nodiscard]] ChiakiControllerState toChiakiControllerState(const GamepadSample& sample);

// Combines two sources exactly like chiaki's chiaki_controller_state_or: the
// buttons add up, the triggers take the stronger pull, each axis the larger
// deflection, the first down touch fills each slot, and the whole sensor
// block comes from the first state that isn't at rest.
[[nodiscard]] ChiakiControllerState mergeControllerStates(ChiakiControllerState first,
                                                          ChiakiControllerState second);

} // namespace lynx::input
