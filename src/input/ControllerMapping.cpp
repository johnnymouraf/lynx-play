#include "input/ControllerMapping.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace lynx::input
{

namespace
{

struct ButtonMapping
{
    std::uint32_t gamepadButton;
    ChiakiControllerButton chiakiButton;
};

constexpr std::array<ButtonMapping, 14> kButtonMappings{{
    {buttons::kCross, CHIAKI_CONTROLLER_BUTTON_CROSS},
    {buttons::kCircle, CHIAKI_CONTROLLER_BUTTON_MOON},
    {buttons::kSquare, CHIAKI_CONTROLLER_BUTTON_BOX},
    {buttons::kTriangle, CHIAKI_CONTROLLER_BUTTON_PYRAMID},
    {buttons::kDpadLeft, CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT},
    {buttons::kDpadRight, CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT},
    {buttons::kDpadUp, CHIAKI_CONTROLLER_BUTTON_DPAD_UP},
    {buttons::kDpadDown, CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN},
    {buttons::kL1, CHIAKI_CONTROLLER_BUTTON_L1},
    {buttons::kR1, CHIAKI_CONTROLLER_BUTTON_R1},
    {buttons::kL3, CHIAKI_CONTROLLER_BUTTON_L3},
    {buttons::kR3, CHIAKI_CONTROLLER_BUTTON_R3},
    {buttons::kOptions, CHIAKI_CONTROLLER_BUTTON_OPTIONS},
    {buttons::kCreate, CHIAKI_CONTROLLER_BUTTON_SHARE},
}};

std::uint32_t chiakiButtons(std::uint32_t gamepadButtons)
{
    std::uint32_t result = 0;

    for (const ButtonMapping& mapping : kButtonMappings)
    {
        if ((gamepadButtons & mapping.gamepadButton) != 0)
        {
            result |= static_cast<std::uint32_t>(mapping.chiakiButton);
        }
    }

    if ((gamepadButtons & buttons::kTouchpad) != 0)
    {
        result |= static_cast<std::uint32_t>(CHIAKI_CONTROLLER_BUTTON_TOUCHPAD);
    }

    if ((gamepadButtons & buttons::kPs) != 0)
    {
        result |= static_cast<std::uint32_t>(CHIAKI_CONTROLLER_BUTTON_PS);
    }

    return result;
}

std::uint16_t touchCoordinate(float normalized, std::uint16_t maximum)
{
    const float clamped = std::clamp(normalized, 0.0f, 1.0f);
    return static_cast<std::uint16_t>(std::lround(clamped * static_cast<float>(maximum)));
}

} // namespace

ChiakiControllerState toChiakiControllerState(const GamepadSample& sample)
{    ChiakiControllerState state;
    chiaki_controller_state_set_idle(&state);

    state.buttons = chiakiButtons(sample.buttons);
    state.l2_state = sample.leftTrigger;
    state.r2_state = sample.rightTrigger;
    state.left_x = sample.leftX;
    state.left_y = sample.leftY;
    state.right_x = sample.rightX;
    state.right_y = sample.rightY;

    for (std::size_t i = 0; i < sample.touches.size(); ++i)
    {
        const GamepadTouch& touch = sample.touches[i];

        if (!touch.down)
        {
            continue;
        }

        state.touches[i].id = static_cast<std::int8_t>(i);
        state.touches[i].x = touchCoordinate(touch.x, kTouchpadWidth);
        state.touches[i].y = touchCoordinate(touch.y, kTouchpadHeight);
    }

    state.touch_id_next = static_cast<std::uint8_t>(CHIAKI_CONTROLLER_TOUCHES_MAX);

    state.gyro_x = sample.gyro[0];
    state.gyro_y = sample.gyro[1];
    state.gyro_z = sample.gyro[2];
    state.accel_x = sample.accel[0];
    state.accel_y = sample.accel[1];
    state.accel_z = sample.accel[2];
    state.orient_x = sample.orientation[0];
    state.orient_y = sample.orientation[1];
    state.orient_z = sample.orientation[2];
    state.orient_w = sample.orientation[3];

    return state;
}

GamepadSample mergeSamples(const GamepadSample& first, const GamepadSample& second)
{
    const auto strongerAxis = [](std::int16_t left, std::int16_t right)
    { return std::abs(left) >= std::abs(right) ? left : right; };

    GamepadSample merged = first;
    merged.buttons |= second.buttons;
    merged.leftTrigger = std::max(first.leftTrigger, second.leftTrigger);
    merged.rightTrigger = std::max(first.rightTrigger, second.rightTrigger);
    merged.leftX = strongerAxis(first.leftX, second.leftX);
    merged.leftY = strongerAxis(first.leftY, second.leftY);
    merged.rightX = strongerAxis(first.rightX, second.rightX);
    merged.rightY = strongerAxis(first.rightY, second.rightY);

    if (!merged.hasMotion && second.hasMotion)
    {
        merged.touches = second.touches;
        merged.gyro = second.gyro;
        merged.accel = second.accel;
        merged.orientation = second.orientation;
        merged.hasMotion = true;
    }

    return merged;
}

} // namespace lynx::input
