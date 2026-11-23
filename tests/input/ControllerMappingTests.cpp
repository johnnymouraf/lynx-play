#include "input/ControllerMapping.h"

#include <array>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace lynx::input;

TEST_CASE("Every SDL button maps to the control chiaki-ng reads")
{
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_SOUTH) == buttons::kCross);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_EAST) == buttons::kCircle);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_WEST) == buttons::kSquare);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_NORTH) == buttons::kTriangle);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_DPAD_LEFT) == buttons::kDpadLeft);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_DPAD_RIGHT) == buttons::kDpadRight);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_DPAD_UP) == buttons::kDpadUp);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_DPAD_DOWN) == buttons::kDpadDown);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) == buttons::kL1);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) == buttons::kR1);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_LEFT_STICK) == buttons::kL3);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_RIGHT_STICK) == buttons::kR3);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_START) == buttons::kOptions);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_BACK) == buttons::kCreate);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_TOUCHPAD) == buttons::kTouchpad);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_GUIDE) == buttons::kPs);

    // The microphone is a mute toggle, and the Edge paddles aren't used.
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_MISC1) == 0);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1) == 0);
    CHECK(gamepadButtonMask(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2) == 0);
}

TEST_CASE("toChiakiControllerState maps every face, shoulder and menu button")
{
    GamepadSample sample;
    sample.buttons = buttons::kCross | buttons::kCircle | buttons::kSquare | buttons::kTriangle;

    CHECK(toChiakiControllerState(sample).buttons ==
          (CHIAKI_CONTROLLER_BUTTON_CROSS | CHIAKI_CONTROLLER_BUTTON_MOON |
           CHIAKI_CONTROLLER_BUTTON_BOX | CHIAKI_CONTROLLER_BUTTON_PYRAMID));

    sample.buttons = buttons::kDpadLeft | buttons::kDpadRight | buttons::kDpadUp |
                     buttons::kDpadDown | buttons::kL1 | buttons::kR1 | buttons::kL3 |
                     buttons::kR3 | buttons::kOptions | buttons::kCreate;

    CHECK(toChiakiControllerState(sample).buttons ==
          (CHIAKI_CONTROLLER_BUTTON_DPAD_LEFT | CHIAKI_CONTROLLER_BUTTON_DPAD_RIGHT |
           CHIAKI_CONTROLLER_BUTTON_DPAD_UP | CHIAKI_CONTROLLER_BUTTON_DPAD_DOWN |
           CHIAKI_CONTROLLER_BUTTON_L1 | CHIAKI_CONTROLLER_BUTTON_R1 |
           CHIAKI_CONTROLLER_BUTTON_L3 | CHIAKI_CONTROLLER_BUTTON_R3 |
           CHIAKI_CONTROLLER_BUTTON_OPTIONS | CHIAKI_CONTROLLER_BUTTON_SHARE));

    sample.buttons = buttons::kTouchpad | buttons::kPs;

    CHECK(toChiakiControllerState(sample).buttons ==
          (CHIAKI_CONTROLLER_BUTTON_TOUCHPAD | CHIAKI_CONTROLLER_BUTTON_PS));
}

TEST_CASE("The pressed controls are named as chiaki names")
{
    CHECK(buttonNames(0).empty());
    CHECK(buttonNames(CHIAKI_CONTROLLER_BUTTON_CROSS | CHIAKI_CONTROLLER_BUTTON_MOON) ==
          "CROSS MOON");
    CHECK(buttonNames(CHIAKI_CONTROLLER_BUTTON_TOUCHPAD | CHIAKI_CONTROLLER_BUTTON_PS) ==
          "TOUCHPAD PS");
    CHECK(buttonNames(static_cast<std::uint32_t>(CHIAKI_CONTROLLER_BUTTON_SHARE)) == "SHARE");
}

TEST_CASE("A controller at rest has no buttons and centered sticks")
{
    const ChiakiControllerState state = toChiakiControllerState(GamepadSample{});

    CHECK(state.buttons == 0);
    CHECK(state.left_x == 0);
    CHECK(state.left_y == 0);
    CHECK(state.right_x == 0);
    CHECK(state.right_y == 0);
    CHECK(state.l2_state == 0);
    CHECK(state.r2_state == 0);
    CHECK(state.touches[0].id == -1);
    CHECK(state.touches[1].id == -1);
    CHECK(state.touch_id_next == 0);
    CHECK(state.accel_y == 1.0F);
}

TEST_CASE("Sticks and triggers keep their scale")
{
    GamepadSample sample;
    sample.leftX = -12345;
    sample.leftY = 23456;
    sample.rightX = -32768;
    sample.rightY = 32767;
    sample.leftTrigger = 42;
    sample.rightTrigger = 255;

    const ChiakiControllerState state = toChiakiControllerState(sample);

    CHECK(state.left_x == -12345);
    CHECK(state.left_y == 23456);
    CHECK(state.right_x == -32768);
    CHECK(state.right_y == 32767);
    CHECK(state.l2_state == 42);
    CHECK(state.r2_state == 255);
}

TEST_CASE("The SDL3 trigger range 0..32767 becomes the console's 0..255")
{
    CHECK(triggerLevel(-32768) == 0);
    CHECK(triggerLevel(0) == 0);
    CHECK(triggerLevel(8191) == 63);
    CHECK(triggerLevel(16383) == 127);
    CHECK(triggerLevel(32767) == 255);
}

TEST_CASE("Touchpad points scale to the PS5's coordinates")
{
    GamepadSample sample;
    applyTouch(sample, 0, TouchPhase::Down, 0.0F, 0.0F);
    applyTouch(sample, 1, TouchPhase::Down, 0.5F, 1.0F);

    ChiakiControllerState state = toChiakiControllerState(sample);

    CHECK(state.touches[0].id == 0);
    CHECK(state.touches[0].x == 0);
    CHECK(state.touches[0].y == 0);
    CHECK(state.touches[1].id == 1);
    CHECK(state.touches[1].x == 960);
    CHECK(state.touches[1].y == 1079);
    CHECK(state.touch_id_next == 2);

    applyTouch(sample, 0, TouchPhase::Up, 0.0F, 0.0F);
    state = toChiakiControllerState(sample);

    CHECK(state.touches[0].id == -1);
    CHECK(state.touches[1].id == 1);
}

TEST_CASE("Each touch takes a new id, as chiaki allocates them")
{
    GamepadSample sample;
    applyTouch(sample, 0, TouchPhase::Down, 0.25F, 0.25F);
    CHECK(sample.touches[0].id == 0);
    CHECK(sample.touchIdNext == 1);

    applyTouch(sample, 1, TouchPhase::Down, 0.75F, 0.75F);
    CHECK(sample.touches[1].id == 1);
    CHECK(sample.touchIdNext == 2);

    applyTouch(sample, 0, TouchPhase::Up, 0.25F, 0.25F);
    applyTouch(sample, 0, TouchPhase::Down, 0.5F, 0.5F);
    CHECK(sample.touches[0].id == 2);
    CHECK(sample.touchIdNext == 3);
}

TEST_CASE("Touchpad points outside the pad are clamped")
{
    GamepadSample sample;
    applyTouch(sample, 0, TouchPhase::Down, 1.5F, -0.5F);

    const ChiakiControllerState state = toChiakiControllerState(sample);

    CHECK(state.touches[0].x == kTouchpadWidth);
    CHECK(state.touches[0].y == 0);
}

TEST_CASE("The accelerometer's meters per second squared become g")
{
    const std::array<float, 3> rest = accelerationInG({0.0F, 9.80665F, 0.0F});
    const std::array<float, 3> tilted = accelerationInG({-9.80665F, 0.0F, 4.903325F});

    CHECK(rest[0] == 0.0F);
    CHECK(rest[1] == 1.0F);
    CHECK(rest[2] == 0.0F);
    CHECK(tilted[0] == -1.0F);
    CHECK(tilted[1] == 0.0F);
    CHECK(tilted[2] == Catch::Approx(0.5F));
}

TEST_CASE("Gyroscope, accelerometer and orientation reach the console")
{
    GamepadSample sample;
    sample.gyro = {1.5F, -2.0F, 3.0F};
    sample.accel = {0.0F, 1.0F, 0.0F};
    sample.orientation = {0.1F, 0.2F, 0.3F, 0.9F};
    sample.hasMotion = true;

    const ChiakiControllerState state = toChiakiControllerState(sample);

    CHECK(state.gyro_x == 1.5F);
    CHECK(state.gyro_y == -2.0F);
    CHECK(state.gyro_z == 3.0F);
    CHECK(state.accel_x == 0.0F);
    CHECK(state.accel_y == 1.0F);
    CHECK(state.accel_z == 0.0F);
    CHECK(state.orient_x == 0.1F);
    CHECK(state.orient_y == 0.2F);
    CHECK(state.orient_z == 0.3F);
    CHECK(state.orient_w == 0.9F);
}
