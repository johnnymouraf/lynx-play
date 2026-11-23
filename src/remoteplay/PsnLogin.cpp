#include "remoteplay/PsnLogin.h"

#include "core/Text.h"
#include "remoteplay/Base64.h"

#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <utility>

#include <nlohmann/json.hpp>

namespace lynx::remoteplay
{

namespace
{

using Json = nlohmann::json;

constexpr std::string_view kUnreserved =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
constexpr std::string_view kHexDigits = "0123456789ABCDEF";
constexpr std::uint32_t kHighNibbleShift = 4U;
constexpr std::uint32_t kLowNibbleMask = 0x0FU;

std::optional<std::uint8_t> hexValue(char character)
{
    if (character >= '0' && character <= '9')
    {
        return static_cast<std::uint8_t>(character - '0');
    }

    if (character >= 'a' && character <= 'f')
    {
        return static_cast<std::uint8_t>(character - 'a' + 10);
    }

    if (character >= 'A' && character <= 'F')
    {
        return static_cast<std::uint8_t>(character - 'A' + 10);
    }

    return std::nullopt;
}

std::string decodeComponent(std::string_view text)
{
    std::string decoded;
    decoded.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '+')
        {
            decoded += ' ';
            continue;
        }

        if (text[i] == '%' && i + 2 < text.size())
        {
            const auto high = hexValue(text[i + 1]);
            const auto low = hexValue(text[i + 2]);

            if (high && low)
            {
                decoded += static_cast<char>((*high << kHighNibbleShift) | *low);
                i += 2;
                continue;
            }
        }

        decoded += text[i];
    }

    return decoded;
}

std::string formEncode(std::string_view text)
{
    std::string encoded;
    encoded.reserve(text.size());

    for (const char character : text)
    {
        if (kUnreserved.find(character) != std::string_view::npos)
        {
            encoded += character;
            continue;
        }

        const auto value = static_cast<std::uint32_t>(static_cast<unsigned char>(character));
        encoded += '%';
        encoded += kHexDigits[value >> kHighNibbleShift];
        encoded += kHexDigits[value & kLowNibbleMask];
    }

    return encoded;
}

// The query or fragment after the redirect page, without the fragment of a
// query.
std::optional<std::string_view> parametersOf(std::string_view url)
{
    const std::size_t question = url.find('?');

    if (question != std::string_view::npos)
    {
        const std::size_t fragment = url.find('#', question + 1);
        const std::size_t count =
            fragment == std::string_view::npos ? std::string_view::npos : fragment - question - 1;
        return url.substr(question + 1, count);
    }

    const std::size_t fragment = url.find('#');

    if (fragment != std::string_view::npos)
    {
        return url.substr(fragment + 1);
    }

    return std::nullopt;
}

} // namespace

std::string psnAuthorizeUrl()
{
    return std::format(
        "{}?service_entity=urn:service-entity:psn&response_type=code&client_id={}"
        "&redirect_uri={}&scope={}&request_locale=en_US&ui=pr&service_logo=ps"
        "&layout_type=popup&smcid=remoteplay&prompt=always&PlatformPrivacyWs1=minimal&",
        kPsnAuthorizeUrl, kPsnClientId, kPsnRedirectUri, kPsnScope);
}

std::string psnBasicAuthHeader()
{
    const std::string credentials = std::format("{}:{}", kPsnClientId, kPsnClientSecret);
    const auto bytes = std::span(reinterpret_cast<const std::uint8_t*>(credentials.data()),
                                 credentials.size());

    return "Basic " + encodeBase64(bytes);
}

Redirect parseRedirect(std::string_view url)
{
    const std::string_view trimmed = core::trimmed(url);
    const auto parameters = parametersOf(trimmed);

    if (!parameters)
    {
        return {};
    }

    std::string code;
    std::string error;
    std::size_t start = 0;

    while (start <= parameters->size())
    {
        const std::size_t end = parameters->find('&', start);
        const std::string_view pair = parameters->substr(
            start, end == std::string_view::npos ? std::string_view::npos : end - start);
        const std::size_t equals = pair.find('=');

        if (equals != std::string_view::npos)
        {
            const std::string name = decodeComponent(pair.substr(0, equals));
            const std::string value = decodeComponent(pair.substr(equals + 1));

            if (name == "code" && !value.empty())
            {
                code = value;
            }
            else if (name == "error" && !value.empty())
            {
                error = value;
            }
        }

        if (end == std::string_view::npos)
        {
            break;
        }

        start = end + 1;
    }

    if (!code.empty())
    {
        return Redirect{.status = RedirectStatus::Code, .code = std::move(code), .error = {}};
    }

    if (error == "access_denied")
    {
        return Redirect{.status = RedirectStatus::AccessDenied, .error = std::move(error)};
    }

    if (!error.empty())
    {
        return Redirect{.status = RedirectStatus::Rejected, .error = std::move(error)};
    }

    return {};
}

std::expected<PsnTokenResponse, TokenResponseError> parseTokenResponse(std::string_view json)
{
    const Json root = Json::parse(json, nullptr, false);

    if (root.is_discarded() || !root.is_object())
    {
        return std::unexpected(TokenResponseError::NotJson);
    }

    try
    {
        const auto access = root.find("access_token");

        if (access == root.end() || !access->is_string() || access->get<std::string>().empty())
        {
            return std::unexpected(TokenResponseError::MissingAccessToken);
        }

        const auto expiry = root.find("expires_in");

        if (expiry == root.end() || !expiry->is_number_integer())
        {
            return std::unexpected(TokenResponseError::InvalidExpiry);
        }

        const std::int64_t seconds = expiry->get<std::int64_t>();

        if (seconds <= 0)
        {
            return std::unexpected(TokenResponseError::InvalidExpiry);
        }

        PsnTokenResponse response;
        response.accessToken = access->get<std::string>();
        response.expiresIn = std::chrono::seconds(seconds);

        if (const auto refresh = root.find("refresh_token");
            refresh != root.end() && refresh->is_string())
        {
            response.refreshToken = refresh->get<std::string>();
        }

        return response;
    }
    catch (const Json::exception&)
    {
        return std::unexpected(TokenResponseError::NotJson);
    }
}

std::string authorizationCodeBody(std::string_view code)
{
    return std::format("grant_type=authorization_code&code={}&scope={}&redirect_uri={}",
                       formEncode(code), kPsnScope, kPsnRedirectUri);
}

std::string refreshTokenBody(std::string_view refreshToken)
{
    return std::format("grant_type=refresh_token&refresh_token={}&scope={}&redirect_uri={}",
                       formEncode(refreshToken), kPsnScope, kPsnRedirectUri);
}

std::expected<std::string, UserInfoError> parseUserIdResponse(std::string_view json)
{
    const Json root = Json::parse(json, nullptr, false);

    if (root.is_discarded() || !root.is_object())
    {
        return std::unexpected(UserInfoError::NotJson);
    }

    try
    {
        const auto userId = root.find("user_id");

        if (userId == root.end())
        {
            return std::unexpected(UserInfoError::MissingUserId);
        }

        if (userId->is_string())
        {
            const std::string text = userId->get<std::string>();

            if (!text.empty())
            {
                return text;
            }
        }
        else if (userId->is_number_integer())
        {
            return userId->dump();
        }
    }
    catch (const Json::exception&)
    {
        return std::unexpected(UserInfoError::MissingUserId);
    }

    return std::unexpected(UserInfoError::MissingUserId);
}

std::expected<AccountId, AccountIdError> accountIdFromUserId(std::string_view userId)
{
    return parseAccountId(userId);
}

} // namespace lynx::remoteplay
