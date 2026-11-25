#include "render/StatisticsLayout.h"

#include <algorithm>

namespace mfplay::render
{

namespace
{

// The column starts of a greedy fill that opens a new column when the next
// section would pass `availableHeight`.
std::vector<std::size_t> fillColumns(std::span<const float> sectionHeights,
                                     float availableHeight, float gap)
{
    std::vector<std::size_t> starts{0};
    float used = 0.0F;

    for (std::size_t index = 0; index < sectionHeights.size(); ++index)
    {
        const bool columnIsEmpty = starts.back() == index;
        const float needed = columnIsEmpty ? sectionHeights[index] : gap + sectionHeights[index];

        if (!columnIsEmpty && used + needed > availableHeight)
        {
            starts.push_back(index);
            used = sectionHeights[index];
            continue;
        }

        used += needed;
    }

    return starts;
}

} // namespace

std::vector<std::size_t> packColumns(std::span<const float> sectionHeights,
                                     float availableHeight, float gap, std::size_t maxColumns)
{
    if (sectionHeights.empty())
    {
        return {0};
    }

    std::vector<std::size_t> starts = fillColumns(sectionHeights, availableHeight, gap);

    if (starts.size() > maxColumns)
    {
        starts.resize(std::max<std::size_t>(maxColumns, 1));
    }

    return starts;
}

std::size_t columnsThatFit(float availableWidth, float columnWidth, float gap)
{
    if (columnWidth <= 0.0F || availableWidth < columnWidth)
    {
        return 1;
    }

    return 1 + static_cast<std::size_t>((availableWidth - columnWidth) / (columnWidth + gap));
}

} // namespace mfplay::render
