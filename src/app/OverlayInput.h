#pragma once

#include "input/ControllerMapping.h"
#include "render/Overlay.h"

namespace mfplay::app
{

namespace overlay_input
{

[[nodiscard]] constexpr std::uint32_t mask(ChiakiControllerButton button)
{
    return static_cast<std::uint32_t>(button);
}

} // namespace overlay_input

// The controller state the overlay's menu navigates with, and the debug
// view's snapshot of what the console receives.
[[nodiscard]] inline render::OverlayGamepad overlayGamepad(const ChiakiControllerState& state)
{
    using overlay_input::mask;

    return {
        .connected = true,
        .cross = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_CROSS)) != 0,
        .circle = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_MOON)) != 0,
        .square = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_BOX)) != 0,
        .triangle = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_PYRAMID)) != 0,
        .dpadUp = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_DPAD_UP)) != 0,
        .dpadDown = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN)) != 0,
        .dpadLeft = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT)) != 0,
        .dpadRight = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT)) != 0,
        .l1 = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_L1)) != 0,
        .r1 = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_R1)) != 0,
        .l3 = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_L3)) != 0,
        .r3 = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_R3)) != 0,
        .options = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_OPTIONS)) != 0,
        .create = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_SHARE)) != 0,
        .touchpad = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_TOUCHPAD)) != 0,
        .ps = (state.buttons & mask(CHIAKI_CONTROLLER_BUTTON_PS)) != 0,
        .leftX = static_cast<float>(state.left_x) / 32767.0F,
        .leftY = static_cast<float>(state.left_y) / 32767.0F,
        .rightX = static_cast<float>(state.right_x) / 32767.0F,
        .rightY = static_cast<float>(state.right_y) / 32767.0F,
        .buttons = input::buttonNames(state.buttons),
        .leftTrigger = state.l2_state,
        .rightTrigger = state.r2_state,
        .touches = {{
            {
                .down = state.touches[0].id >= 0,
                .x = state.touches[0].x,
                .y = state.touches[0].y,
            },
            {
                .down = state.touches[1].id >= 0,
                .x = state.touches[1].x,
                .y = state.touches[1].y,
            },
        }},
        .gyro = {state.gyro_x, state.gyro_y, state.gyro_z},
        .accel = {state.accel_x, state.accel_y, state.accel_z},
    };
}

} // namespace mfplay::app
