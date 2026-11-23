#include "remoteplay/PsnTokenStore.h"

#include "core/Files.h"
#include "remoteplay/Base64.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <istream>
#include <ostream>
#include <span>
#include <sstream>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace lynx::remoteplay
{

namespace
{

using Json = nlohmann::ordered_json;

constexpr int kJsonIndent = 2;
constexpr std::size_t kExpiryBytes = 8;
constexpr std::uint32_t kBitsPerByte = 8U;
constexpr std::uint64_t kByteMask = 0xFFU;

std::span<const std::uint8_t> bytesOf(std::string_view text)
{
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

std::expected<std::string, std::string> sealText(std::string_view text,
                                                 const SecretProtector& protector)
{
    const auto sealed = protector.protect(bytesOf(text));

    if (!sealed)
    {
        return std::unexpected(sealed.error());
    }

    return encodeBase64(*sealed);
}

std::expected<std::string, std::string> openText(const std::string& text,
                                                 const SecretProtector& protector)
{
    const auto sealed = decodeBase64(text);

    if (!sealed)
    {
        return std::unexpected(std::string("not Base64"));
    }

    const auto plain = protector.unprotect(*sealed);

    if (!plain)
    {
        return std::unexpected(plain.error());
    }

    return std::string(plain->begin(), plain->end());
}

std::expected<std::string, std::string> sealExpiry(Clock::time_point time,
                                                   const SecretProtector& protector)
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch());
    const auto value = static_cast<std::uint64_t>(seconds.count());
    std::array<std::uint8_t, kExpiryBytes> bytes{};

    for (std::size_t index = 0; index < kExpiryBytes; ++index)
    {
        bytes.at(index) =
            static_cast<std::uint8_t>((value >> (kBitsPerByte * index)) & kByteMask);
    }

    const auto sealed = protector.protect(bytes);

    if (!sealed)
    {
        return std::unexpected(sealed.error());
    }

    return encodeBase64(*sealed);
}

std::expected<Clock::time_point, std::string> openExpiry(const std::string& text,
                                                         const SecretProtector& protector)
{
    const auto sealed = decodeBase64(text);

    if (!sealed)
    {
        return std::unexpected(std::string("not Base64"));
    }

    const auto plain = protector.unprotect(*sealed);

    if (!plain)
    {
        return std::unexpected(plain.error());
    }

    if (plain->size() != kExpiryBytes)
    {
        return std::unexpected(std::string("wrong size"));
    }

    std::uint64_t value = 0;

    for (std::size_t index = 0; index < kExpiryBytes; ++index)
    {
        value |= static_cast<std::uint64_t>(plain->at(index)) << (kBitsPerByte * index);
    }

    return Clock::time_point(std::chrono::seconds(static_cast<std::int64_t>(value)));
}

std::expected<PsnTokens, std::string> tokensFromJson(const Json& root,
                                                     const SecretProtector& protector)
{
    if (!root.is_object() || !root.contains("version") || !root.contains("accessToken") ||
        !root.contains("refreshToken"))
    {
        return std::unexpected(std::string("not a Lynx Play token file"));
    }

    const int version = root.at("version").get<int>();

    if (version != kPsnTokenFileVersion)
    {
        return std::unexpected(std::format("unsupported version {}", version));
    }

    const auto accessToken = openText(root.at("accessToken").get<std::string>(), protector);

    if (!accessToken)
    {
        return std::unexpected(accessToken.error());
    }

    const auto refreshToken = openText(root.at("refreshToken").get<std::string>(), protector);

    if (!refreshToken)
    {
        return std::unexpected(refreshToken.error());
    }

    PsnTokens tokens{
        .accessToken = *accessToken,
        .refreshToken = *refreshToken,
        .expiry = std::nullopt,
    };

    if (root.contains("expiry"))
    {
        const auto expiry = openExpiry(root.at("expiry").get<std::string>(), protector);

        if (!expiry)
        {
            return std::unexpected(expiry.error());
        }

        tokens.expiry = *expiry;
    }

    return tokens;
}

} // namespace

std::expected<PsnTokens, std::string> readPsnTokens(std::istream& input,
                                                    const SecretProtector& protector)
{
    const Json root = Json::parse(input, nullptr, false);

    if (root.is_discarded())
    {
        return std::unexpected(std::string("not valid JSON"));
    }

    try
    {
        return tokensFromJson(root, protector);
    }
    catch (const Json::exception& exception)
    {
        return std::unexpected(std::string("unexpected content: ") + exception.what());
    }
}

std::expected<void, std::string> writePsnTokens(const PsnTokens& tokens, std::ostream& output,
                                                const SecretProtector& protector)
{
    if (tokens.accessToken.empty() || tokens.refreshToken.empty())
    {
        return std::unexpected(std::string("no tokens to write"));
    }

    const auto accessToken = sealText(tokens.accessToken, protector);

    if (!accessToken)
    {
        return std::unexpected(accessToken.error());
    }

    const auto refreshToken = sealText(tokens.refreshToken, protector);

    if (!refreshToken)
    {
        return std::unexpected(refreshToken.error());
    }

    Json root{
        {"version", kPsnTokenFileVersion},
        {"accessToken", *accessToken},
        {"refreshToken", *refreshToken},
    };

    if (tokens.expiry)
    {
        const auto expiry = sealExpiry(*tokens.expiry, protector);

        if (!expiry)
        {
            return std::unexpected(expiry.error());
        }

        root["expiry"] = *expiry;
    }

    output << root.dump(kJsonIndent) << '\n';

    if (!output)
    {
        return std::unexpected(std::string("cannot write the tokens"));
    }

    return {};
}

PsnTokenStore::PsnTokenStore(std::filesystem::path path, SecretProtector protector)
    : path_(std::move(path)), protector_(std::move(protector))
{
}

PsnTokens PsnTokenStore::load() const
{
    std::error_code error;

    if (!std::filesystem::exists(path_, error))
    {
        return {};
    }

    std::ifstream input(path_, std::ios::binary);

    if (!input)
    {
        spdlog::error("Cannot open {}", path_.string());
        return {};
    }

    const auto tokens = readPsnTokens(input, protector_);

    if (!tokens)
    {
        spdlog::warn("{} is unreadable ({}); the PSN sign-in is forgotten", path_.string(),
                     tokens.error());
        return {};
    }

    return *tokens;
}

std::expected<void, std::string> PsnTokenStore::save(const PsnTokens& tokens) const
{
    std::ostringstream buffer;

    if (auto written = writePsnTokens(tokens, buffer, protector_); !written)
    {
        return std::unexpected(written.error());
    }

    return core::replaceFile(path_, buffer.str());
}

} // namespace lynx::remoteplay
