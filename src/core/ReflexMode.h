#pragma once

#include <string_view>

namespace mfplay::core
{

// NVIDIA Reflex's modes, as settings.json stores them.
enum class ReflexMode
{
    Off,
    // Trims the render queue's latency.
    On,
    // On plus the driver's highest GPU clocks; raises a laptop's power use.
    Boost,
};

[[nodiscard]] constexpr std::string_view reflexModeName(ReflexMode mode)
{
    switch (mode)
    {
    case ReflexMode::Off:
        return "off";

    case ReflexMode::On:
        return "on";

    case ReflexMode::Boost:
        return "boost";
    }

    return "off";
}

} // namespace mfplay::core
