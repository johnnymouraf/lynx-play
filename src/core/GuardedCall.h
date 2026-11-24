#pragma once

#include <cstdint>
#include <functional>

namespace mfplay::core
{

// Runs a call into a driver so an access violation in it is reported in
// `fault` instead of taking the process down; false when the call faulted.
// The call runs inside a structured exception handler, so it must not need
// C++ unwinding of its own.
[[nodiscard]] bool callGuarded(const std::function<void()>& call, std::uint32_t& fault);

} // namespace mfplay::core
