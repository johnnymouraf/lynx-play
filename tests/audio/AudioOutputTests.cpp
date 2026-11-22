#include "audio/AudioOutput.h"

#include <catch2/catch_test_macros.hpp>

using namespace lynx::audio;

TEST_CASE("The audio queue holds at most 60 ms at 48 kHz stereo")
{
    CHECK(audioQueueCap(48000, 2) == 11520);
    CHECK(audioQueueCap(44100, 2) == 10584);
}

TEST_CASE("Audio is dropped only when the queue is over its cap")
{
    constexpr std::size_t cap = audioQueueCap(48000, 2);

    CHECK_FALSE(shouldDropAudio(0, cap));
    CHECK_FALSE(shouldDropAudio(-1, cap));
    CHECK_FALSE(shouldDropAudio(static_cast<int>(cap), cap));
    CHECK(shouldDropAudio(static_cast<int>(cap) + 1, cap));
}
