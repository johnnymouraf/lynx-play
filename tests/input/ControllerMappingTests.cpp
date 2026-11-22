#include "input/ControllerMapping.h"

#include <catch2/catch_test_macros.hpp>

using namespace lynx::input;

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

TEST_CASE("The SDL trigger range becomes the console's 0..255")
{
    CHECK(triggerLevel(-32768) == 0);
    CHECK(triggerLevel(-16384) == 64);
    CHECK(triggerLevel(0) == 128);
    CHECK(triggerLevel(16383) == 191);
    CHECK(triggerLevel(32767) == 255);
}

TEST_CASE("Touchpad points scale to the PS5's coordinates and keep one id per finger")
{
    GamepadSample sample;
    sample.touches[0] = {.down = true, .x = 0.0f, .y = 0.0f};
    sample.touches[1] = {.down = true, .x = 0.5f, .y = 1.0f};

    ChiakiControllerState state = toChiakiControllerState(sample);

    CHECK(state.touches[0].id == 0);
    CHECK(state.touches[0].x == 0);
    CHECK(state.touches[0].y == 0);
    CHECK(state.touches[1].id == 1);
    CHECK(state.touches[1].x == 960);
    CHECK(state.touches[1].y == 1079);

    sample.touches[0].down = false;
    state = toChiakiControllerState(sample);

    CHECK(state.touches[0].id == -1);
    CHECK(state.touches[1].id == 1);
}

TEST_CASE("Touchpad points outside the pad are clamped")
{
    GamepadSample sample;
    sample.touches[0] = {.down = true, .x = 1.5f, .y = -0.5f};

    const ChiakiControllerState state = toChiakiControllerState(sample);

    CHECK(state.touches[0].x == kTouchpadWidth);
    CHECK(state.touches[0].y == 0);
}

TEST_CASE("Gyroscope, accelerometer and orientation reach the console")
{
    GamepadSample sample;
    sample.gyro = {1.5f, -2.0f, 3.0f};
    sample.accel = {0.0f, 1.0f, 0.0f};
    sample.orientation = {0.1f, 0.2f, 0.3f, 0.9f};
    sample.hasMotion = true;

    const ChiakiControllerState state = toChiakiControllerState(sample);

    CHECK(state.gyro_x == 1.5f);
    CHECK(state.gyro_y == -2.0f);
    CHECK(state.gyro_z == 3.0f);
    CHECK(state.accel_x == 0.0f);
    CHECK(state.accel_y == 1.0f);
    CHECK(state.accel_z == 0.0f);
    CHECK(state.orient_x == 0.1f);
    CHECK(state.orient_y == 0.2f);
    CHECK(state.orient_z == 0.3f);
    CHECK(state.orient_w == 0.9f);
}
