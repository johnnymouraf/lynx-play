#include "remoteplay/AccountId.h"
#include "remoteplay/PsnLogin.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace mfplay::remoteplay;

namespace
{

constexpr std::string_view kRedirectPrefix =
    "https://remoteplay.dl.playstation.net/remoteplay/redirect";

} // namespace

TEST_CASE("The PSN sign-in page is chiaki's authorize URL")
{
    const std::string url = psnAuthorizeUrl();

    CHECK(url ==
          "https://auth.api.sonyentertainmentnetwork.com/2.0/oauth/authorize"
          "?service_entity=urn:service-entity:psn&response_type=code"
          "&client_id=ba495a24-818c-472b-b12d-ff231c1b5745"
          "&redirect_uri=https://remoteplay.dl.playstation.net/remoteplay/redirect"
          "&scope=psn:clientapp referenceDataService:countryConfig.read "
          "pushNotification:webSocket.desktop.connect "
          "sessionManager:remotePlaySession.system.update"
          "&request_locale=en_US&ui=pr&service_logo=ps&layout_type=popup"
          "&smcid=remoteplay&prompt=always&PlatformPrivacyWs1=minimal&");
    CHECK(url.starts_with(kPsnAuthorizeUrl));
    CHECK(url.find("prompt=always") != std::string::npos);
    CHECK(url.find("response_type=code") != std::string::npos);
}

TEST_CASE("The token requests use the client id and secret as Basic auth")
{
    CHECK(psnBasicAuthHeader() ==
          "Basic YmE0OTVhMjQtODE4Yy00NzJiLWIxMmQtZmYyMzFjMWI1NzQ1Om12YWlaa1JzQXNJMUlCa1k=");
}

TEST_CASE("The redirect parser reads the authorization code")
{
    CHECK(parseRedirect("https://remoteplay.dl.playstation.net/remoteplay/redirect?code=abc123") ==
          Redirect{.status = RedirectStatus::Code, .code = "abc123", .error = {}});
    CHECK(parseRedirect("  https://remoteplay.dl.playstation.net/remoteplay/redirect?"
                        "cid=x&code=a%2Fb%2Bc&state=y  ") ==
          Redirect{.status = RedirectStatus::Code, .code = "a/b+c", .error = {}});
    CHECK(parseRedirect("https://remoteplay.dl.playstation.net/remoteplay/redirect#code=xyz") ==
          Redirect{.status = RedirectStatus::Code, .code = "xyz", .error = {}});
}

TEST_CASE("The redirect parser tells a canceled sign-in from the others")
{
    CHECK(parseRedirect("https://remoteplay.dl.playstation.net/remoteplay/redirect?"
                        "error=access_denied") ==
          Redirect{.status = RedirectStatus::AccessDenied, .error = "access_denied"});
    CHECK(parseRedirect("https://remoteplay.dl.playstation.net/remoteplay/redirect?"
                        "error=invalid_request") ==
          Redirect{.status = RedirectStatus::Rejected, .error = "invalid_request"});
}

TEST_CASE("The redirect parser rejects a URL without a code or an error")
{
    CHECK(parseRedirect("") == Redirect{});
    CHECK(parseRedirect("   ") == Redirect{});
    CHECK(parseRedirect(kRedirectPrefix) == Redirect{});
    CHECK(parseRedirect("https://remoteplay.dl.playstation.net/remoteplay/redirect?state=1") ==
          Redirect{});
    CHECK(parseRedirect("not a url") == Redirect{});
}

TEST_CASE("The token response carries both tokens and the lifetime")
{
    const auto response = parseTokenResponse(
        R"({"access_token":"access","refresh_token":"refresh","expires_in":3600})");

    REQUIRE(response);
    CHECK(*response == PsnTokenResponse{.accessToken = "access",
                                        .refreshToken = "refresh",
                                        .expiresIn = std::chrono::seconds(3600)});
}

TEST_CASE("The token response takes a rotation without a new refresh token")
{
    const auto response = parseTokenResponse(R"({"access_token":"new","expires_in":60})");

    REQUIRE(response);
    CHECK(response->accessToken == "new");
    CHECK(response->refreshToken.empty());
    CHECK(response->expiresIn == std::chrono::seconds(60));
}

TEST_CASE("The token response names what is missing or invalid")
{
    CHECK(parseTokenResponse("not json") ==
          std::unexpected(TokenResponseError::NotJson));
    CHECK(parseTokenResponse(R"({"refresh_token":"r","expires_in":60})") ==
          std::unexpected(TokenResponseError::MissingAccessToken));
    CHECK(parseTokenResponse(R"({"access_token":"","expires_in":60})") ==
          std::unexpected(TokenResponseError::MissingAccessToken));
    CHECK(parseTokenResponse(R"({"access_token":"a","refresh_token":"r"})") ==
          std::unexpected(TokenResponseError::InvalidExpiry));
    CHECK(parseTokenResponse(R"({"access_token":"a","expires_in":"60"})") ==
          std::unexpected(TokenResponseError::InvalidExpiry));
    CHECK(parseTokenResponse(R"({"access_token":"a","expires_in":0})") ==
          std::unexpected(TokenResponseError::InvalidExpiry));
}

TEST_CASE("The code and refresh bodies carry chiaki's parameters")
{
    const std::string codeBody = authorizationCodeBody("abc-123");

    CHECK(codeBody ==
          "grant_type=authorization_code&code=abc-123"
          "&scope=psn:clientapp referenceDataService:countryConfig.read "
          "pushNotification:webSocket.desktop.connect "
          "sessionManager:remotePlaySession.system.update"
          "&redirect_uri=https://remoteplay.dl.playstation.net/remoteplay/redirect");

    const std::string refreshBody = refreshTokenBody("refresh token+/=");

    CHECK(refreshBody ==
          "grant_type=refresh_token&refresh_token=refresh%20token%2B%2F%3D"
          "&scope=psn:clientapp referenceDataService:countryConfig.read "
          "pushNotification:webSocket.desktop.connect "
          "sessionManager:remotePlaySession.system.update"
          "&redirect_uri=https://remoteplay.dl.playstation.net/remoteplay/redirect");
}

TEST_CASE("The user info response gives the user ID")
{
    const auto userId = parseUserIdResponse(
        R"({"user_id":"1234567890123456789","online_id":"someone"})");

    REQUIRE(userId);
    CHECK(*userId == "1234567890123456789");
    CHECK(parseUserIdResponse(R"({"user_id":258})") == std::string("258"));
    CHECK(parseUserIdResponse(R"({"online_id":"someone"})") ==
          std::unexpected(UserInfoError::MissingUserId));
    CHECK(parseUserIdResponse("not json") == std::unexpected(UserInfoError::NotJson));
}

TEST_CASE("The user ID becomes the pairing's Account ID")
{
    // The fixtures are made up; a real user ID never goes into the repository.
    const auto small = accountIdFromUserId("258");
    const auto large = accountIdFromUserId("1234567890123456789");

    REQUIRE(small);
    REQUIRE(large);
    CHECK(formatAccountId(*small) == "AgEAAAAAAAA=");
    CHECK(formatAccountId(*large) == "FYHpffQQIhE=");
    CHECK(accountIdFromUserId("0") == std::unexpected(AccountIdError::InvalidNumber));
    CHECK(accountIdFromUserId("") == std::unexpected(AccountIdError::Empty));
    CHECK(accountIdFromUserId("not a number") ==
          std::unexpected(AccountIdError::InvalidFormat));
}
