#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace lynx::render
{

inline constexpr std::uint32_t kNvidiaVendorId = 0x10DE;

struct AdapterInfo
{
    std::string name;
    std::uint32_t vendorId = 0;
    bool isSoftware = false;
};

// Picks the adapter to render the stream on from a list in DXGI's
// high-performance order: the first NVIDIA hardware adapter, since the RTX
// features need one, otherwise the first hardware adapter. Software adapters
// (Microsoft Basic Render Driver) are never picked.
[[nodiscard]] std::optional<std::size_t> selectAdapter(std::span<const AdapterInfo> adapters);

} // namespace lynx::render
