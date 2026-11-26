#pragma once

#include <expected>
#include <functional>
#include <string>

namespace mfplay::remoteplay
{

using DuidGenerator = std::function<std::expected<std::string, std::string>()>;

struct ClientDuid
{
    std::string value;
    // True when `value` was just made and still has to be saved.
    bool generated = false;

    bool operator==(const ClientDuid&) const = default;
};

// The saved identifier when there is one, otherwise a new one from `generate`.
[[nodiscard]] std::expected<ClientDuid, std::string> resolveClientDuid(
    const std::string& saved, const DuidGenerator& generate);

// A new identifier from chiaki-lib.
[[nodiscard]] std::expected<std::string, std::string> generateClientDuid();

} // namespace mfplay::remoteplay
