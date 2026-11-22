#pragma once

#include <expected>
#include <string>

namespace lynx::remoteplay
{

// Initializes chiaki-lib's process-wide state (randomness, FEC tables,
// Winsock, certificate fetching). Call once before any other chiaki API; it
// has no matching shutdown.
[[nodiscard]] std::expected<void, std::string> initializeChiaki();

} // namespace lynx::remoteplay
