#include "core/GuardedCall.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using lynx::core::callGuarded;

namespace
{

constexpr std::uint32_t kAccessViolation = 0xC0000005UL;

void writeToNull()
{
    *static_cast<volatile int*>(nullptr) = 1;
}

} // namespace

TEST_CASE("A guard reports a driver fault instead of taking the process down")
{
    std::uint32_t fault = 0;
    CHECK_FALSE(callGuarded(writeToNull, fault));
    CHECK(fault == kAccessViolation);

    fault = 0;
    CHECK(callGuarded([] {}, fault));
    CHECK(fault == 0);
}
