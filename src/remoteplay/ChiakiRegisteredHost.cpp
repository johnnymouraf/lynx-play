#include "remoteplay/ChiakiRegisteredHost.h"

#include <cstring>

namespace mfplay::remoteplay
{

PairedConsole pairedConsoleOf(const ChiakiRegisteredHost& host)
{
    PairedConsole paired;
    std::memcpy(paired.credentials.registKey.data(), host.rp_regist_key, kRegistKeySize);
    std::memcpy(paired.credentials.rpKey.data(), host.rp_key, kRpKeySize);
    paired.credentials.rpKeyType = host.rp_key_type;
    std::memcpy(paired.serverMac.data(), host.server_mac, kMacSize);

    const std::size_t nicknameLength = strnlen(host.server_nickname, sizeof(host.server_nickname));
    paired.serverNickname.assign(host.server_nickname, nicknameLength);
    paired.target = static_cast<int>(host.target);

    return paired;
}

} // namespace mfplay::remoteplay
