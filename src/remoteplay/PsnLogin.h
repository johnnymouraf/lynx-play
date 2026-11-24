#pragma once

#include "remoteplay/AccountId.h"

#include <chrono>
#include <expected>
#include <string>
#include <string_view>

namespace mfplay::remoteplay
{

// Sony's PSN OAuth client, as chiaki-ng registers it: the same client,
// redirect, scope and endpoints the console pairing flow uses.
inline constexpr std::string_view kPsnClientId = "ba495a24-818c-472b-b12d-ff231c1b5745";
inline constexpr std::string_view kPsnClientSecret = "mvaiZkRsAsI1IBkY";
inline constexpr std::string_view kPsnRedirectUri =
    "https://remoteplay.dl.playstation.net/remoteplay/redirect";
inline constexpr std::string_view kPsnScope =
    "psn:clientapp referenceDataService:countryConfig.read "
    "pushNotification:webSocket.desktop.connect "
    "sessionManager:remotePlaySession.system.update";
inline constexpr std::string_view kPsnAuthorizeUrl =
    "https://auth.api.sonyentertainmentnetwork.com/2.0/oauth/authorize";
inline constexpr std::string_view kPsnTokenUrl =
    "https://auth.api.sonyentertainmentnetwork.com/2.0/oauth/token";

// The page the system browser opens to sign in, with chiaki-ng's query.
[[nodiscard]] std::string psnAuthorizeUrl();

// "Basic <Base64 of client id and secret>", for the token endpoints.
[[nodiscard]] std::string psnBasicAuthHeader();

enum class RedirectStatus
{
    // The page carries the authorization code.
    Code,
    // The user canceled on Sony's page.
    AccessDenied,
    // Sony answered with another error, such as invalid_request.
    Rejected,
    // The text has no code and no error.
    Malformed,
};

struct Redirect
{
    RedirectStatus status = RedirectStatus::Malformed;
    std::string code;
    std::string error;

    bool operator==(const Redirect&) const = default;
};

// Reads the URL the browser lands on after the PSN sign-in.
[[nodiscard]] Redirect parseRedirect(std::string_view url);

struct PsnTokenResponse
{
    std::string accessToken;
    // Empty when Sony didn't send a new one; a refresh keeps the previous.
    std::string refreshToken;
    std::chrono::seconds expiresIn{};

    bool operator==(const PsnTokenResponse&) const = default;
};

enum class TokenResponseError
{
    NotJson,
    MissingAccessToken,
    InvalidExpiry,
};

// Reads the token endpoint's response.
[[nodiscard]] std::expected<PsnTokenResponse, TokenResponseError> parseTokenResponse(
    std::string_view json);

// The form body that exchanges the redirect code for tokens.
[[nodiscard]] std::string authorizationCodeBody(std::string_view code);

// The form body that trades a refresh token for a new access token.
[[nodiscard]] std::string refreshTokenBody(std::string_view refreshToken);

enum class UserInfoError
{
    NotJson,
    MissingUserId,
};

// The user ID from the token info endpoint, which the account ID comes from.
[[nodiscard]] std::expected<std::string, UserInfoError> parseUserIdResponse(
    std::string_view json);

// The pairing's Account ID: the user ID's 8 little-endian bytes, Base64.
[[nodiscard]] std::expected<AccountId, AccountIdError> accountIdFromUserId(
    std::string_view userId);

} // namespace mfplay::remoteplay
