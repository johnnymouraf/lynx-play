#include "audio/Haptics.h"

#include <array>

#include <catch2/catch_test_macros.hpp>

using namespace lynx::audio;

TEST_CASE("Haptics route to audio on USB, to rumble on Bluetooth and off without a controller")
{
    CHECK(chooseHapticsRoute(false, true, true) == HapticsRoute::Off);
    CHECK(chooseHapticsRoute(false, false, false) == HapticsRoute::Off);
    CHECK(chooseHapticsRoute(true, true, true) == HapticsRoute::Audio);
    CHECK(chooseHapticsRoute(true, true, false) == HapticsRoute::Rumble);
    CHECK(chooseHapticsRoute(true, false, false) == HapticsRoute::Rumble);
    CHECK(chooseHapticsRoute(true, false, true) == HapticsRoute::Rumble);
}

TEST_CASE("Haptics stereo becomes four channels with the actuator pair carrying the signal")
{
    const std::array<std::int16_t, 4> stereo{100, 200, -300, 400};
    const std::vector<std::int16_t> quad = upmixHaptics(stereo);

    REQUIRE(quad.size() == 8);
    CHECK(quad[0] == 0);
    CHECK(quad[1] == 0);
    CHECK(quad[2] == 100);
    CHECK(quad[3] == 200);
    CHECK(quad[4] == 0);
    CHECK(quad[5] == 0);
    CHECK(quad[6] == -300);
    CHECK(quad[7] == 400);
}

TEST_CASE("The haptic gain scales and clamps the actuator signal")
{
    const std::array<std::int16_t, 2> stereo{1000, -2000};
    const std::vector<std::int16_t> half = upmixHaptics(stereo, 0.5f);
    const std::vector<std::int16_t> loud = upmixHaptics(stereo, 100.0f);

    CHECK(half[2] == 500);
    CHECK(half[3] == -1000);
    CHECK(loud[2] == 32767);
    CHECK(loud[3] == -32768);
}

TEST_CASE("The rumble fallback follows the loudest channel and ignores near silence")
{
    const std::array<std::int16_t, 2> silence{0, 0};
    const std::array<std::int16_t, 2> quiet{10, -20};
    const std::array<std::int16_t, 2> loud{1000, -2000};
    const std::array<std::int16_t, 2> maximum{-32768, 32767};

    CHECK(hapticRumbleLevel(silence) == 0);
    CHECK(hapticRumbleLevel(quiet) == 0);
    CHECK(hapticRumbleLevel(loud) == 4000);
    CHECK(hapticRumbleLevel(maximum) == 65535);
}

TEST_CASE("The rumble fallback threshold is configurable")
{
    const std::array<std::int16_t, 2> quiet{10, -20};

    CHECK(hapticRumbleLevel(quiet, 5) == 40);
}
