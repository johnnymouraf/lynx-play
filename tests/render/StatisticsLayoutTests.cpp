#include "render/StatisticsLayout.h"

#include <array>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using mfplay::render::columnsThatFit;
using mfplay::render::packColumns;

using Starts = std::vector<std::size_t>;

TEST_CASE("packColumns keeps one column when every section fits")
{
    const std::array<float, 3> heights{100.0F, 100.0F, 100.0F};

    CHECK(packColumns(heights, 400.0F, 10.0F, 3) == Starts{0});
}

TEST_CASE("packColumns opens a column when the next section would overflow")
{
    const std::array<float, 4> heights{100.0F, 100.0F, 100.0F, 100.0F};

    CHECK(packColumns(heights, 250.0F, 10.0F, 4) == Starts{0, 2});
}

TEST_CASE("packColumns counts the gap between sections")
{
    const std::array<float, 2> heights{100.0F, 100.0F};

    CHECK(packColumns(heights, 205.0F, 10.0F, 2) == Starts{0, 1});
    CHECK(packColumns(heights, 210.0F, 10.0F, 2) == Starts{0});
}

TEST_CASE("packColumns never leaves a column empty for a section taller than the space")
{
    const std::array<float, 3> heights{500.0F, 50.0F, 50.0F};

    CHECK(packColumns(heights, 200.0F, 10.0F, 3) == Starts{0, 1});
}

TEST_CASE("packColumns stops at the columns the width allows")
{
    const std::array<float, 4> heights{100.0F, 100.0F, 100.0F, 100.0F};

    CHECK(packColumns(heights, 100.0F, 10.0F, 2) == Starts{0, 1});
}

TEST_CASE("columnsThatFit counts whole columns and gaps, at least one")
{
    CHECK(columnsThatFit(100.0F, 320.0F, 24.0F) == 1);
    CHECK(columnsThatFit(320.0F, 320.0F, 24.0F) == 1);
    CHECK(columnsThatFit(664.0F, 320.0F, 24.0F) == 2);
    CHECK(columnsThatFit(1000.0F, 320.0F, 24.0F) == 2);
    CHECK(columnsThatFit(1008.0F, 320.0F, 24.0F) == 3);
}
