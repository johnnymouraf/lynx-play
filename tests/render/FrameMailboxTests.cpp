#include "render/FrameMailbox.h"

#include <catch2/catch_test_macros.hpp>

using lynx::render::FrameMailbox;
using lynx::render::VideoFrame;

namespace
{

// A frame whose lease records when the renderer lets go of it.
VideoFrame frameNumbered(std::uint32_t width, bool& released)
{
    return {
        .width = width,
        .lease = std::shared_ptr<const void>(nullptr,
                                             [&released](const void*) { released = true; }),
    };
}

} // namespace

TEST_CASE("FrameMailbox hands over only the newest frame")
{
    auto mailbox = FrameMailbox::create();
    REQUIRE(mailbox.has_value());
    bool firstReleased = false;
    bool secondReleased = false;

    (*mailbox)->publish(frameNumbered(1, firstReleased));
    (*mailbox)->publish(frameNumbered(2, secondReleased));
    const auto taken = (*mailbox)->take();

    REQUIRE(taken.has_value());
    CHECK(taken->width == 2);
    CHECK(firstReleased);
    CHECK_FALSE(secondReleased);
    CHECK((*mailbox)->takeDroppedCount() == 1U);
}

TEST_CASE("FrameMailbox is empty once its frame is taken")
{
    auto mailbox = FrameMailbox::create();
    REQUIRE(mailbox.has_value());
    bool released = false;

    (*mailbox)->publish(frameNumbered(1, released));
    REQUIRE((*mailbox)->take().has_value());

    CHECK_FALSE((*mailbox)->take().has_value());
    CHECK((*mailbox)->takeDroppedCount() == 0U);
}
