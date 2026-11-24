#pragma once

#include "remoteplay/Console.h"

#include <array>
#include <cstdint>
#include <expected>
#include <string>

namespace mfplay::remoteplay
{

// The credential a wake-up packet carries: the registration key, which holds
// hex digits, read as a number.
[[nodiscard]] std::expected<std::uint64_t, std::string> wakeCredential(
    const std::array<std::uint8_t, kRegistKeySize>& registKey);

// Sends the wake-up packet that turns on a paired console in rest mode.
[[nodiscard]] std::expected<void, std::string> sendWakeup(const std::string& address,
                                                         std::uint64_t credential, bool ps5);

} // namespace mfplay::remoteplay
