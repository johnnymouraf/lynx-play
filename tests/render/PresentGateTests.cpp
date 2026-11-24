#include "render/PresentGate.h"

#include <catch2/catch_test_macros.hpp>

using mfplay::render::PresentGate;
using mfplay::render::PresentOutcome;
using mfplay::render::SurfaceSize;

namespace
{

constexpr SurfaceSize kWindow{.width = 1920, .height = 1080};
constexpr SurfaceSize kMinimized{};

} // namespace

TEST_CASE("A shown window may present")
{
    PresentGate gate;
    gate.setClientSize(kWindow);

    CHECK(gate.mayPresent());
}

TEST_CASE("An empty client area parks the gate until the window returns")
{
    PresentGate gate;
    gate.setClientSize(kWindow);

    gate.setClientSize(kMinimized);

    CHECK_FALSE(gate.mayPresent());
    CHECK_FALSE(gate.mayPresent());

    gate.setClientSize(kWindow);

    CHECK(gate.mayPresent());
}

TEST_CASE("An occluded chain skips presents and retries periodically")
{
    PresentGate gate;
    gate.setClientSize(kWindow);
    gate.observe(PresentOutcome::Occluded);

    for (std::uint32_t frame = 1; frame < PresentGate::kOccludedRetryFrames; ++frame)
    {
        CHECK_FALSE(gate.mayPresent());
    }

    CHECK(gate.mayPresent());

    gate.observe(PresentOutcome::Presented);

    CHECK(gate.mayPresent());
}

TEST_CASE("A restore clears an occlusion the swap chain reported")
{
    PresentGate gate;
    gate.setClientSize(kWindow);
    gate.observe(PresentOutcome::Occluded);

    CHECK_FALSE(gate.mayPresent());

    gate.setClientSize(kMinimized);
    gate.setClientSize(kWindow);

    CHECK(gate.mayPresent());
}
