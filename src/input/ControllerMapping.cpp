#include "input/ControllerMapping.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace lynx::input
{

namespace
{

struct ButtonEntry
{
    SDL_GamepadButton sdl;
    std::uint32_t mask;
};

constexpr std::array<ButtonEntry, 16> kButtons{{
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
    {SDL_GAMEPAD_BUTTON_GUIDE, buttons::kPs},
}};

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

struct ButtonName
{
    std::uint32_t gamepadButton;
    const char* name;
};

constexpr std::array<ButtonName, 16> kButtonNames{{
    {buttons::kCross, "CROSS"},
    {buttons::kCircle, "MOON"},
    {buttons::kSquare, "BOX"},
    {buttons::kTriangle, "PYRAMID"},
    {buttons::kDpadLeft, "DPAD_LEFT"},
    {buttons::kDpadRight, "DPAD_RIGHT"},
    {buttons::kDpadUp, "DPAD_UP"},
    {buttons::kDpadDown, "DPAD_DOWN"},
    {buttons::kL1, "L1"},
    {buttons::kR1, "R1"},
    {buttons::kL3, "L3"},
    {buttons::kR3, "R3"},
    {buttons::kOptions, "OPTIONS"},
    {buttons::kCreate, "SHARE"},
    {buttons::kTouchpad, "TOUCHPAD"},
    {buttons::kPs, "PS"},
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

} // namespace

std::uint16_t touchCoordinate(float normalized, std::uint16_t maximum)
{
    const float clamped = std::clamp(normalized, 0.0f, 1.0f);
    return static_cast<std::uint16_t>(clamped * static_cast<float>(maximum));
}

void applyTouch(GamepadSample& sample, int finger, TouchPhase phase, float x, float y)
{
    if (finger < 0 || finger >= static_cast<int>(sample.touches.size()))
    {
        return;
    }

    GamepadTouch& touch = sample.touches[static_cast<std::size_t>(finger)];

    switch (phase)
    {
    case TouchPhase::Down:
        if (!touch.down)
        {
            touch.id = static_cast<std::int8_t>(sample.touchIdNext);
            sample.touchIdNext = static_cast<std::uint8_t>((sample.touchIdNext + 1) &
                                                           touch_id::kMask);
            touch.down = true;
        }

        touch.x = x;
        touch.y = y;
        return;

    case TouchPhase::Motion:
        if (!touch.down)
        {
            return;
        }

        touch.x = x;
        touch.y = y;
        return;

    case TouchPhase::Up:
        touch.down = false;
        touch.id = -1;
        return;
    }
}

std::uint32_t gamepadButtonMask(SDL_GamepadButton button)
{
    const auto found = std::ranges::find(kButtons, button, &ButtonEntry::sdl);
    return found != kButtons.end() ? found->mask : 0;
}

std::string buttonNames(std::uint32_t chiakiButtonsValue)
{
    std::string names;

    for (const ButtonName& entry : kButtonNames)
    {
        if ((chiakiButtonsValue & entry.gamepadButton) == 0)
        {
            continue;
        }

        if (!names.empty())
        {
            names += ' ';
        }

        names += entry.name;
    }

    return names;
}

ChiakiControllerState toChiakiControllerState(const GamepadSample& sample)
{
    ChiakiControllerState state;
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

        state.touches[i].id = touch.id;
        state.touches[i].x = touchCoordinate(touch.x, kTouchpadWidth);
        state.touches[i].y = touchCoordinate(touch.y, kTouchpadHeight);
    }

    state.touch_id_next = sample.touchIdNext;

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

    for (std::size_t i = 0; i < merged.touches.size(); ++i)
    {
        if (!merged.touches[i].down && second.touches[i].down)
        {
            merged.touches[i] = second.touches[i];
            merged.touchIdNext = second.touchIdNext;
        }
    }

    if (!merged.hasMotion && second.hasMotion)
    {
        merged.gyro = second.gyro;
        merged.accel = second.accel;
        merged.orientation = second.orientation;
        merged.hasMotion = true;
    }

    return merged;
}

} // namespace lynx::input
