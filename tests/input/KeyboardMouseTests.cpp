#include "input/ControllerMapping.h"
#include "input/KeyboardMouse.h"

#include <catch2/catch_test_macros.hpp>

using namespace mfplay::input;
using mfplay::core::KeyboardMouseSettings;

namespace
{

bool hasButton(const GamepadSample& sample, std::uint32_t button)
{
    return (sample.buttons & button) != 0;
}

bool hasControllerButton(const ChiakiControllerState& state, ChiakiControllerButton button)
{
    return (state.buttons & static_cast<std::uint32_t>(button)) != 0;
}

} // namespace

TEST_CASE("The default keyboard plays the controller")
{
    KeyboardMouseInput input{KeyboardMouseSettings{}};

    input.keyDown('W');
    input.keyDown('A');
    input.keyDown('Q');
    input.keyDown('R');

    const GamepadSample sample = input.sample();

    CHECK(sample.leftY == -32767);
    CHECK(sample.leftX == -32767);
    CHECK(hasButton(sample, buttons::kCircle));
    CHECK(hasButton(sample, buttons::kSquare));

    input.keyUp('W');
    input.keyUp('A');
    CHECK(input.sample().leftY == 0);
    CHECK(input.sample().leftX == 0);
}

TEST_CASE("Mouse buttons press the triggers and the buttons they are bound to")
{
    KeyboardMouseInput input{KeyboardMouseSettings{}};

    input.mouseButton(mfplay::core::kMouseRight, true);
    input.mouseButton(mfplay::core::kMouseLeft, true);

    const GamepadSample sample = input.sample();

    CHECK(sample.leftTrigger == 255);
    CHECK(sample.rightTrigger == 255);
}

TEST_CASE("Mouse movement becomes the right stick with the chosen sensitivity")
{
    KeyboardMouseSettings settings;
    KeyboardMouseInput input{settings};

    input.mouseMoved(25, 10);
    const GamepadSample normal = input.sample();

    CHECK(normal.rightX == 25000);
    CHECK(normal.rightY == 10000);

    // The movement is consumed by the sample.
    CHECK(input.sample().rightX == 0);

    settings.mouseSensitivity = 2.0;
    settings.invertY = true;
    input.setSettings(settings);
    input.mouseMoved(10, 10);
    const GamepadSample fast = input.sample();

    CHECK(fast.rightX == 20000);
    CHECK(fast.rightY == -20000);
}

TEST_CASE("The overlay's menu keeps keyboard and mouse input from the game")
{
    KeyboardMouseInput input{KeyboardMouseSettings{}};

    input.keyDown('W');
    input.mouseMoved(50, 0);
    input.setOverlayOpen(true);

    const GamepadSample sample = input.sample();

    CHECK(sample.buttons == 0);
    CHECK(sample.leftY == 0);
    CHECK(sample.rightX == 0);

    input.setOverlayOpen(false);
    CHECK(input.sample().leftY == -32767);
}

TEST_CASE("Losing the window's focus releases every key")
{
    KeyboardMouseInput input{KeyboardMouseSettings{}};
    input.keyDown('W');
    input.keyDown('Q');
    input.mouseButton(mfplay::core::kMouseLeft, true);
    input.mouseMoved(10, 10);

    input.releaseAll();
    const GamepadSample sample = input.sample();

    CHECK(sample.buttons == 0);
    CHECK(sample.leftY == 0);
    CHECK(sample.rightTrigger == 0);
    CHECK(sample.rightX == 0);
}

TEST_CASE("A disabled keyboard plays nothing")
{
    KeyboardMouseSettings settings;
    settings.enabled = false;
    KeyboardMouseInput input{settings};

    input.keyDown('W');
    input.mouseButton(mfplay::core::kMouseLeft, true);
    input.mouseMoved(50, 0);

    const GamepadSample sample = input.sample();

    CHECK(sample.buttons == 0);
    CHECK(sample.leftY == 0);
    CHECK(sample.rightTrigger == 0);
    CHECK(sample.rightX == 0);

    settings.enabled = true;
    input.setSettings(settings);
    CHECK(input.sample().leftY == -32767);
}

TEST_CASE("A second input source adds to the controller's sample")
{
    GamepadSample gamepad;
    gamepad.buttons = buttons::kCross;
    gamepad.leftX = 10000;
    gamepad.rightTrigger = 100;

    GamepadSample keyboard;
    keyboard.buttons = buttons::kCircle;
    keyboard.leftX = -20000;
    keyboard.leftY = 30000;
    keyboard.rightTrigger = 255;

    const ChiakiControllerState merged = mergeControllerStates(
        toChiakiControllerState(gamepad), toChiakiControllerState(keyboard));

    CHECK(hasControllerButton(merged, CHIAKI_CONTROLLER_BUTTON_CROSS));
    CHECK(hasControllerButton(merged, CHIAKI_CONTROLLER_BUTTON_MOON));
    CHECK(merged.left_x == -20000);
    CHECK(merged.left_y == 30000);
    CHECK(merged.r2_state == 255);
}
