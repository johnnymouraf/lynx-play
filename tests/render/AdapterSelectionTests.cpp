#include "render/AdapterSelection.h"

#include <array>

#include <catch2/catch_test_macros.hpp>

using mfplay::render::AdapterInfo;
using mfplay::render::selectAdapter;

namespace
{

constexpr std::uint32_t kIntelVendorId = 0x8086;
constexpr std::uint32_t kAmdVendorId = 0x1002;
constexpr std::uint32_t kMicrosoftVendorId = 0x1414;

const AdapterInfo kIntelIntegrated{"Intel(R) UHD Graphics", kIntelVendorId, false};
const AdapterInfo kRtx4060{"NVIDIA GeForce RTX 4060 Laptop GPU", mfplay::render::kNvidiaVendorId,
                           false};
const AdapterInfo kRadeon{"AMD Radeon RX 7600", kAmdVendorId, false};
const AdapterInfo kBasicRender{"Microsoft Basic Render Driver", kMicrosoftVendorId, true};

} // namespace

TEST_CASE("selectAdapter prefers the NVIDIA GPU even when another comes first")
{
    const std::array adapters{kIntelIntegrated, kRtx4060, kBasicRender};

    CHECK(selectAdapter(adapters) == 1U);
}

TEST_CASE("selectAdapter falls back to the first hardware adapter without NVIDIA")
{
    const std::array adapters{kBasicRender, kRadeon, kIntelIntegrated};

    CHECK(selectAdapter(adapters) == 1U);
}

TEST_CASE("selectAdapter finds nothing when only software adapters exist")
{
    const std::array adapters{kBasicRender};

    CHECK_FALSE(selectAdapter(adapters).has_value());
}
