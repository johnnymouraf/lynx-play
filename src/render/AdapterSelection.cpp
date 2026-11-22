#include "render/AdapterSelection.h"

#include <algorithm>

namespace lynx::render
{

std::optional<std::size_t> selectAdapter(std::span<const AdapterInfo> adapters)
{
    const auto isHardware = [](const AdapterInfo& adapter) { return !adapter.isSoftware; };
    const auto isNvidiaHardware = [](const AdapterInfo& adapter)
    { return !adapter.isSoftware && adapter.vendorId == kNvidiaVendorId; };

    auto chosen = std::ranges::find_if(adapters, isNvidiaHardware);

    if (chosen == adapters.end())
    {
        chosen = std::ranges::find_if(adapters, isHardware);
    }

    if (chosen == adapters.end())
    {
        return std::nullopt;
    }

    return static_cast<std::size_t>(std::ranges::distance(adapters.begin(), chosen));
}

} // namespace lynx::render
