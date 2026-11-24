#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace mfplay::remoteplay
{

using SecretBytes = std::vector<std::uint8_t>;
using SecretResult = std::expected<SecretBytes, std::string>;

// Encrypts the secrets written to disk and decrypts them back.
struct SecretProtector
{
    std::function<SecretResult(std::span<const std::uint8_t> plain)> protect;
    std::function<SecretResult(std::span<const std::uint8_t> sealed)> unprotect;
};

// Windows DPAPI bound to the current user: only this user, on this PC, can
// decrypt what it encrypts.
[[nodiscard]] SecretProtector dpapiProtector();

} // namespace mfplay::remoteplay
