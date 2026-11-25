#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace mfplay::render
{

// Splits the panel's sections, in order, into side-by-side columns: the fewest
// columns (up to `maxColumns`) where each one's height, with `gap` between
// sections, fits `availableHeight`. When even `maxColumns` do not fit, the
// result uses all of them. Returns the index of the first section of each
// column.
[[nodiscard]] std::vector<std::size_t> packColumns(std::span<const float> sectionHeights,
                                                   float availableHeight, float gap,
                                                   std::size_t maxColumns);

// How many columns of `columnWidth`, `gap` apart, fit in `availableWidth`; at
// least one.
[[nodiscard]] std::size_t columnsThatFit(float availableWidth, float columnWidth, float gap);

} // namespace mfplay::render
