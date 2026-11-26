#include "remoteplay/ClientDuid.h"

#include <catch2/catch_test_macros.hpp>

using namespace mfplay::remoteplay;

TEST_CASE("The client identifier is generated once and reused afterwards")
{
    int generated = 0;
    const DuidGenerator generator = [&generated]() -> std::expected<std::string, std::string> {
        ++generated;
        return "fake-duid";
    };

    const auto first = resolveClientDuid("", generator);
    REQUIRE(first.has_value());
    CHECK(first->value == "fake-duid");
    CHECK(first->generated);

    const auto second = resolveClientDuid(first->value, generator);
    REQUIRE(second.has_value());
    CHECK(second->value == "fake-duid");
    CHECK_FALSE(second->generated);
    CHECK(generated == 1);
}

TEST_CASE("A generator that fails or gives nothing leaves no identifier")
{
    const auto failed = resolveClientDuid(
        "", [] { return std::expected<std::string, std::string>(std::unexpected("no entropy")); });
    const auto empty =
        resolveClientDuid("", [] { return std::expected<std::string, std::string>(""); });

    REQUIRE_FALSE(failed.has_value());
    CHECK(failed.error() == "no entropy");
    CHECK_FALSE(empty.has_value());
}
