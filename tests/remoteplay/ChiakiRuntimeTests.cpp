#include "remoteplay/ChiakiRuntime.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("initializeChiaki brings up chiaki-lib and its dependencies")
{
    const auto result = mfplay::remoteplay::initializeChiaki();

    if (!result)
    {
        FAIL(result.error());
    }
}
