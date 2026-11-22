#pragma once

#include "input/ControllerMapping.h"
#include "render/Overlay.h"

namespace lynx::app
{

// The controller state the overlay's menu navigates with.
[[nodiscard]] inline render::OverlayGamepad overlayGamepad(const input::GamepadSample& sample)
{
    return {
        .connected = true,
        .cross = (sample.buttons & input::buttons::kCross) != 0,
        .circle = (sample.buttons & input::buttons::kCircle) != 0,
        .square = (sample.buttons & input::buttons::kSquare) != 0,
        .triangle = (sample.buttons & input::buttons::kTriangle) != 0,
        .dpadUp = (sample.buttons & input::buttons::kDpadUp) != 0,
        .dpadDown = (sample.buttons & input::buttons::kDpadDown) != 0,
        .dpadLeft = (sample.buttons & input::buttons::kDpadLeft) != 0,
        .dpadRight = (sample.buttons & input::buttons::kDpadRight) != 0,
        .l1 = (sample.buttons & input::buttons::kL1) != 0,
        .r1 = (sample.buttons & input::buttons::kR1) != 0,
        .l3 = (sample.buttons & input::buttons::kL3) != 0,
        .r3 = (sample.buttons & input::buttons::kR3) != 0,
        .options = (sample.buttons & input::buttons::kOptions) != 0,
        .create = (sample.buttons & input::buttons::kCreate) != 0,
        .touchpad = (sample.buttons & input::buttons::kTouchpad) != 0,
        .ps = (sample.buttons & input::buttons::kPs) != 0,
        .leftX = static_cast<float>(sample.leftX) / 32767.0F,
        .leftY = static_cast<float>(sample.leftY) / 32767.0F,
        .rightX = static_cast<float>(sample.rightX) / 32767.0F,
        .rightY = static_cast<float>(sample.rightY) / 32767.0F,
    };
}

} // namespace lynx::app
