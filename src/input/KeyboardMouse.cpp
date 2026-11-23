#include "input/KeyboardMouse.h"

#include <algorithm>
#include <cmath>

namespace lynx::input
{

namespace
{

using core::KeyboardControl;

// A mouse moving 25 pixels between samples (a normal flick) asks for a full
// stick deflection at the default sensitivity.
constexpr double kMousePixelsToStick = 1000.0;
constexpr double kStickMaximum = 32767.0;

std::uint32_t keyMask(KeyboardControl control)
{
    switch (control)
    {
    case KeyboardControl::Cross:
        return buttons::kCross;
    case KeyboardControl::Circle:
        return buttons::kCircle;
    case KeyboardControl::Square:
        return buttons::kSquare;
    case KeyboardControl::Triangle:
        return buttons::kTriangle;
    case KeyboardControl::L1:
        return buttons::kL1;
    case KeyboardControl::R1:
        return buttons::kR1;
    case KeyboardControl::L2:
    case KeyboardControl::R2:
        // The triggers are analog on the console; the keyboard only has
        // pressed or not.
        return 0;
    case KeyboardControl::Options:
        return buttons::kOptions;
    case KeyboardControl::Create:
        return buttons::kCreate;
    case KeyboardControl::LeftStickUp:
    case KeyboardControl::LeftStickDown:
    case KeyboardControl::LeftStickLeft:
    case KeyboardControl::LeftStickRight:
    case KeyboardControl::RightStick:
    case KeyboardControl::Count:
        return 0;
    }

    return 0;
}

std::int16_t stickValue(double value)
{
    return static_cast<std::int16_t>(std::clamp(value, -kStickMaximum, kStickMaximum));
}

} // namespace

KeyboardMouseInput::KeyboardMouseInput(const core::KeyboardMouseSettings& settings)
    : settings_(settings)
{
    rebuildBindings();
}

void KeyboardMouseInput::setSettings(const core::KeyboardMouseSettings& settings)
{
    const std::scoped_lock lock(mutex_);
    settings_ = settings;
    rebuildBindings();
}

void KeyboardMouseInput::setOverlayOpen(bool open)
{
    overlayOpen_.store(open);
}

void KeyboardMouseInput::keyDown(std::uint16_t virtualKey)
{
    const std::scoped_lock lock(mutex_);

    if (virtualKey < keys_.size())
    {
        keys_.at(virtualKey) = true;
    }
}

void KeyboardMouseInput::keyUp(std::uint16_t virtualKey)
{
    const std::scoped_lock lock(mutex_);

    if (virtualKey < keys_.size())
    {
        keys_.at(virtualKey) = false;
    }
}

void KeyboardMouseInput::mouseButton(std::uint16_t button, bool down)
{
    if (button >= mouseButtons_.size())
    {
        return;
    }

    const std::scoped_lock lock(mutex_);
    mouseButtons_.at(button) = down;
}

void KeyboardMouseInput::mouseMoved(int deltaX, int deltaY)
{
    const std::scoped_lock lock(mutex_);
    deltaX_ += static_cast<double>(deltaX);
    deltaY_ += static_cast<double>(deltaY);
}

void KeyboardMouseInput::releaseAll()
{
    const std::scoped_lock lock(mutex_);
    keys_.fill(false);
    mouseButtons_.fill(false);
    deltaX_ = 0.0;
    deltaY_ = 0.0;
}

GamepadSample KeyboardMouseInput::sample()
{
    const std::scoped_lock lock(mutex_);

    GamepadSample result;

    if (!settings_.enabled || overlayOpen_.load())
    {
        deltaX_ = 0.0;
        deltaY_ = 0.0;
        return result;
    }

    for (std::size_t index = 0; index < core::kKeyboardControlCount; ++index)
    {
        const auto control = static_cast<KeyboardControl>(index);
        const Binding& binding = bindings_.at(index);

        if (binding.kind == core::KeyboardBindingKind::MouseMotion)
        {
            if (control == KeyboardControl::RightStick)
            {
                const double scale = kMousePixelsToStick * settings_.mouseSensitivity;
                const double y = settings_.invertY ? -deltaY_ : deltaY_;

                result.rightX = stickValue(deltaX_ * scale);
                result.rightY = stickValue(y * scale);
            }

            continue;
        }

        if (!pressed(binding))
        {
            continue;
        }

        switch (control)
        {
        case KeyboardControl::LeftStickUp:
            result.leftY = static_cast<std::int16_t>(result.leftY - 32767);
            break;
        case KeyboardControl::LeftStickDown:
            result.leftY = static_cast<std::int16_t>(result.leftY + 32767);
            break;
        case KeyboardControl::LeftStickLeft:
            result.leftX = static_cast<std::int16_t>(result.leftX - 32767);
            break;
        case KeyboardControl::LeftStickRight:
            result.leftX = static_cast<std::int16_t>(result.leftX + 32767);
            break;
        case KeyboardControl::L2:
            result.leftTrigger = 255;
            break;
        case KeyboardControl::R2:
            result.rightTrigger = 255;
            break;
        default:
            result.buttons |= keyMask(control);
            break;
        }
    }

    deltaX_ = 0.0;
    deltaY_ = 0.0;

    return result;
}

void KeyboardMouseInput::rebuildBindings()
{
    for (std::size_t index = 0; index < core::kKeyboardControlCount; ++index)
    {
        const auto binding =
            core::keyboardBindingFromName(settings_.bindings.at(index));

        if (binding)
        {
            bindings_.at(index) = {
                .kind = binding->kind,
                .code = binding->code,
            };
        }
        else
        {
            bindings_.at(index) = {};
        }
    }
}

bool KeyboardMouseInput::pressed(const Binding& binding) const
{
    switch (binding.kind)
    {
    case core::KeyboardBindingKind::Key:
        return binding.code < keys_.size() && keys_.at(binding.code);
    case core::KeyboardBindingKind::MouseButton:
        return binding.code < mouseButtons_.size() && mouseButtons_.at(binding.code);
    case core::KeyboardBindingKind::MouseMotion:
        return false;
    }

    return false;
}

} // namespace lynx::input
