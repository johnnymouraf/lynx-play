#include "remoteplay/ConsoleWake.h"

#include "remoteplay/ChiakiLogSink.h"

#include <algorithm>
#include <charconv>

#include <chiaki/discovery.h>

namespace mfplay::remoteplay
{

namespace
{

constexpr int kHexBase = 16;
constexpr std::size_t kMaxCredentialDigits = 16;

} // namespace

std::expected<std::uint64_t, std::string> wakeCredential(
    const std::array<std::uint8_t, kRegistKeySize>& registKey)
{
    const auto end = std::ranges::find(registKey, std::uint8_t{0});
    std::string digits;

    for (auto byte = registKey.begin(); byte != end; ++byte)
    {
        digits += static_cast<char>(*byte);
    }

    std::uint64_t credential = 0;
    const char* last = digits.data() + digits.size();
    const auto result = std::from_chars(digits.data(), last, credential, kHexBase);

    if (digits.empty() || digits.size() > kMaxCredentialDigits || result.ec != std::errc() ||
        result.ptr != last)
    {
        return std::unexpected(std::string("the registration key holds no credential"));
    }

    return credential;
}

std::expected<void, std::string> sendWakeup(const std::string& address, std::uint64_t credential,
                                            bool ps5)
{
    ChiakiLogSink logSink;
    const ChiakiErrorCode result =
        chiaki_discovery_wakeup(logSink.log(), nullptr, address.c_str(), credential, ps5);

    if (result != CHIAKI_ERR_SUCCESS)
    {
        return std::unexpected(std::string("chiaki_discovery_wakeup failed: ") +
                               chiaki_error_string(result));
    }

    return {};
}

} // namespace mfplay::remoteplay
