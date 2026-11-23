#include "remoteplay/Base64.h"
#include "remoteplay/PsnTokenStore.h"

#include <span>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace lynx::remoteplay;

namespace
{

// Made-up tokens; real ones never go into the repository.
PsnTokens fakeTokens()
{
    return PsnTokens{
        .accessToken = "access-token-fake-1",
        .refreshToken = "refresh-token-fake-2",
        .expiry = Clock::time_point(std::chrono::seconds(1'792'000'000)),
    };
}

SecretResult refuse(std::span<const std::uint8_t>)
{
    return std::unexpected(std::string("refused"));
}

SecretProtector refusingProtector()
{
    return SecretProtector{.protect = refuse, .unprotect = refuse};
}

std::span<const std::uint8_t> bytesOf(std::string_view text)
{
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

} // namespace

TEST_CASE("The token store gives back the tokens it saved")
{
    const PsnTokens tokens = fakeTokens();
    const SecretProtector protector = dpapiProtector();
    std::stringstream stream;

    REQUIRE(writePsnTokens(tokens, stream, protector));
    const auto loaded = readPsnTokens(stream, protector);

    REQUIRE(loaded);
    CHECK(*loaded == tokens);
}

TEST_CASE("The token store keeps the tokens and the expiry encrypted on disk")
{
    const PsnTokens tokens = fakeTokens();
    std::stringstream stream;

    REQUIRE(writePsnTokens(tokens, stream, dpapiProtector()));
    const std::string text = stream.str();

    CHECK(text.find("\"accessToken\"") != std::string::npos);
    CHECK(text.find(tokens.accessToken) == std::string::npos);
    CHECK(text.find(tokens.refreshToken) == std::string::npos);
    CHECK(text.find(encodeBase64(bytesOf(tokens.accessToken))) == std::string::npos);
    CHECK(text.find(encodeBase64(bytesOf(tokens.refreshToken))) == std::string::npos);
}

TEST_CASE("The token store writes nothing when a token can't be encrypted")
{
    std::stringstream stream;

    CHECK_FALSE(writePsnTokens(fakeTokens(), stream, refusingProtector()));
    CHECK(stream.str().empty());
}

TEST_CASE("The token store refuses a file it can't decrypt")
{
    std::stringstream stream;
    REQUIRE(writePsnTokens(fakeTokens(), stream, dpapiProtector()));

    CHECK_FALSE(readPsnTokens(stream, refusingProtector()));
}

TEST_CASE("The token store rejects a corrupt, foreign or incomplete file")
{
    const SecretProtector protector = dpapiProtector();

    for (const std::string& text :
         {std::string("not json"), std::string(R"({"version":2})"),
          std::string(R"({"version":1,"accessToken":"eA=="})")})
    {
        std::istringstream input(text);
        CHECK_FALSE(readPsnTokens(input, protector));
    }
}

TEST_CASE("The token store keeps the sign-in without an expiry")
{
    PsnTokens tokens = fakeTokens();
    tokens.expiry = std::nullopt;
    const SecretProtector protector = dpapiProtector();
    std::stringstream stream;

    REQUIRE(writePsnTokens(tokens, stream, protector));
    const auto loaded = readPsnTokens(stream, protector);

    REQUIRE(loaded);
    CHECK(*loaded == tokens);
}
