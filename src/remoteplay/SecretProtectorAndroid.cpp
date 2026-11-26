#include "remoteplay/SecretProtector.h"

namespace mfplay::remoteplay
{

namespace
{

SecretResult refuse([[maybe_unused]] std::span<const std::uint8_t> bytes)
{
    return std::unexpected(std::string("secret protection is not implemented on this platform yet"));
}

} // namespace

SecretProtector platformSecretProtector()
{
    return SecretProtector{
        .protect = refuse,
        .unprotect = refuse,
    };
}

} // namespace mfplay::remoteplay
