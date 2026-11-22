#pragma once

#include <chiaki/controller.h>

#include <array>
#include <cstdint>

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
    std::array<GamepadTouch, CHIAKI_CONTROLLER_TOUCHES_MAX> touches{};
    // Radians per second.
    std::array<float, 3> gyro{};
    // In g, gravity included.
    std::array<float, 3> accel{};
    // The console reads a quaternion.
    std::array<float, 4> orientation{0.0f, 0.0f, 0.0f, 1.0f};
    bool hasMotion = false;
};

// The PS5's touchpad coordinates.
inline constexpr std::uint16_t kTouchpadWidth = 1919;
inline constexpr std::uint16_t kTouchpadHeight = 1079;

// SDL reports a trigger at rest as -32768 and fully pressed as 32767.
[[nodiscard]] constexpr std::uint8_t triggerLevel(std::int16_t value)
{
    return static_cast<std::uint8_t>((static_cast<std::int32_t>(value) + 32768) >> 8);
}

[[nodiscard]] ChiakiControllerState toChiakiControllerState(const GamepadSample& sample);

// Adds a second input source (the keyboard and mouse) to the controller's
// sample: buttons add up, triggers take the stronger pull and each axis the
// larger deflection.
[[nodiscard]] GamepadSample mergeSamples(const GamepadSample& first, const GamepadSample& second);

} // namespace lynx::input
