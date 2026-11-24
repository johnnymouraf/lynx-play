#pragma once

#include "remoteplay/Console.h"
#include "remoteplay/SecretProtector.h"

#include <expected>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>

namespace mfplay::remoteplay
{

inline constexpr int kPsnTokenFileVersion = 1;

// What psn.json keeps: the OAuth tokens and when the access token expires.
struct PsnTokens
{
    std::string accessToken;
    std::string refreshToken;
    std::optional<Clock::time_point> expiry;

    bool operator==(const PsnTokens&) const = default;
};

// Reads psn.json. A secret that can't be decrypted (a file copied from
// another user or PC) makes the whole file unreadable.
[[nodiscard]] std::expected<PsnTokens, std::string> readPsnTokens(
    std::istream& input, const SecretProtector& protector);

// Writes psn.json with every token and the expiry encrypted. Writes nothing
// when one of them can't be encrypted.
[[nodiscard]] std::expected<void, std::string> writePsnTokens(
    const PsnTokens& tokens, std::ostream& output, const SecretProtector& protector);

// psn.json on disk.
class PsnTokenStore
{
public:
    PsnTokenStore(std::filesystem::path path, SecretProtector protector);

    // A missing or unreadable file gives no tokens.
    [[nodiscard]] PsnTokens load() const;

    // Replaces the file through a temporary one, so a failure leaves the old
    // file whole.
    [[nodiscard]] std::expected<void, std::string> save(const PsnTokens& tokens) const;

private:
    std::filesystem::path path_;
    SecretProtector protector_;
};

} // namespace mfplay::remoteplay
