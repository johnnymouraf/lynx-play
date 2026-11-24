#pragma once

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

} // namespace mfplay::core
