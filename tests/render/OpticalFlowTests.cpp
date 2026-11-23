#include "render/OpticalFlow.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace lynx::render;

namespace
{

constexpr std::uint32_t kAccessViolation = 0xC0000005UL;

void writeToNull()
{
    *static_cast<volatile int*>(nullptr) = 1;
}

} // namespace

TEST_CASE("S10.5 flow vectors become pixels")
{
    CHECK(flowVectorPixels(32, -64).x == Catch::Approx(1.0F));
    CHECK(flowVectorPixels(32, -64).y == Catch::Approx(-2.0F));
    CHECK(flowVectorPixels(16, 0).x == Catch::Approx(0.5F));
    CHECK(flowVectorPixels(0, 0).x == Catch::Approx(0.0F));
    CHECK(flowVectorPixels(-1, 1).x == Catch::Approx(-1.0F / 32.0F));
    CHECK(flowVectorPixels(static_cast<std::int16_t>(32'767), static_cast<std::int16_t>(-32'768))
              .x == Catch::Approx(32'767.0F / 32.0F));
    CHECK(flowVectorPixels(static_cast<std::int16_t>(32'767), static_cast<std::int16_t>(-32'768))
              .y == Catch::Approx(-1'024.0F));
}

TEST_CASE("The vector grid rounds the picture up to whole cells")
{
    CHECK(flowGridExtent({1'920, 1'080}).width == 480);
    CHECK(flowGridExtent({1'920, 1'080}).height == 270);
    CHECK(flowGridExtent({1'919, 1'079}).width == 480);
    CHECK(flowGridExtent({1'919, 1'079}).height == 270);
    CHECK(flowGridExtent({1'921, 1'081}).width == 481);
    CHECK(flowGridExtent({1'921, 1'081}).height == 271);
    CHECK(flowGridExtent({1, 1}).width == 1);
    CHECK(flowGridExtent({1, 1}).height == 1);
    CHECK(flowGridExtent({1'920, 1'080}, 2).width == 960);
    CHECK(flowGridExtent({1'920, 1'080}, 2).height == 540);
}

TEST_CASE("Densify picks the grid cell covering the pixel")
{
    // A 2x2 grid for an 8x6 picture: cell (0, 0) is (4, 3).
    const std::array<std::int16_t, 8> vectors{
        32, 0,    // cell (0, 0): 1, 0 px
        -64, 16,  // cell (1, 0): -2, 0.5 px
        0, -32,   // cell (0, 1): 0, -1 px
        96, -96,  // cell (1, 1): 3, -3 px
    };
    const SurfaceSize grid{.width = 2, .height = 2};
    const SurfaceSize picture{.width = 8, .height = 6};

    const auto topLeft = flowVectorAt(vectors, grid, picture, 0, 0);
    REQUIRE(topLeft.has_value());
    CHECK(topLeft->x == Catch::Approx(1.0F));
    CHECK(topLeft->y == Catch::Approx(0.0F));

    const auto middle = flowVectorAt(vectors, grid, picture, 3, 2);
    REQUIRE(middle.has_value());
    CHECK(middle->x == Catch::Approx(1.0F));

    const auto topRight = flowVectorAt(vectors, grid, picture, 4, 0);
    REQUIRE(topRight.has_value());
    CHECK(topRight->x == Catch::Approx(-2.0F));
    CHECK(topRight->y == Catch::Approx(0.5F));

    const auto bottomLeft = flowVectorAt(vectors, grid, picture, 0, 5);
    REQUIRE(bottomLeft.has_value());
    CHECK(bottomLeft->y == Catch::Approx(-1.0F));

    const auto bottomRight = flowVectorAt(vectors, grid, picture, 7, 5);
    REQUIRE(bottomRight.has_value());
    CHECK(bottomRight->x == Catch::Approx(3.0F));
    CHECK(bottomRight->y == Catch::Approx(-3.0F));

    CHECK_FALSE(flowVectorAt(vectors, grid, picture, 8, 0).has_value());
    CHECK_FALSE(flowVectorAt(vectors, grid, picture, 0, 6).has_value());
    CHECK_FALSE(flowVectorAt(std::array<std::int16_t, 2>{0, 0}, grid, picture, 7, 5).has_value());
}

TEST_CASE("A faulting release step is reported and the steps after it still run")
{
    std::vector<SessionReleaseStep> visited;
    std::vector<SessionReleaseStep> faulted;

    runSessionRelease(
        [&visited](SessionReleaseStep step)
        {
            visited.push_back(step);

            if (step == SessionReleaseStep::ReleaseFlowPictures)
            {
                writeToNull();
            }
        },
        [&faulted](SessionReleaseStep step, std::uint32_t fault)
        {
            CHECK(fault == kAccessViolation);
            faulted.push_back(step);
        });

    const std::vector<SessionReleaseStep> expected(kSessionReleaseSteps.begin(),
                                                   kSessionReleaseSteps.end());
    CHECK(visited == expected);
    REQUIRE(faulted.size() == 1);
    CHECK(faulted.front() == SessionReleaseStep::ReleaseFlowPictures);
}

TEST_CASE("The session waits for the GPU before it unregisters and destroys")
{
    CHECK(kSessionReleaseSteps.front() == SessionReleaseStep::WaitForLastFlow);
    CHECK(kSessionReleaseSteps.back() == SessionReleaseStep::DestroySession);

    const auto position = [](SessionReleaseStep step)
    {
        const auto found = std::ranges::find(kSessionReleaseSteps, step);
        return static_cast<std::size_t>(found - kSessionReleaseSteps.begin());
    };

    CHECK(position(SessionReleaseStep::UnregisterCostResources) <
          position(SessionReleaseStep::ReleaseCostPictures));
    CHECK(position(SessionReleaseStep::UnregisterFlowResources) <
          position(SessionReleaseStep::ReleaseFlowPictures));
    CHECK(position(SessionReleaseStep::UnregisterInputResources) <
          position(SessionReleaseStep::ReleaseInputPictures));
    CHECK(position(SessionReleaseStep::ReleaseInputPictures) <
          position(SessionReleaseStep::DestroySession));

    for (const SessionReleaseStep step : kSessionReleaseSteps)
    {
        CHECK_FALSE(sessionReleaseStepName(step).empty());
    }
}
