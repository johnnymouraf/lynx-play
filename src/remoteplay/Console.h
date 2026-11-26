#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace mfplay::remoteplay
{

// What a console reports about itself when it answers a search.
enum class HostState
{
    Unknown,
    Ready,
    Standby,
};

// A console that answered a search on the network.
struct DiscoveredHost
{
    // The console's MAC address in uppercase hex; it identifies the console.
    std::string hostId;
    std::string name;
    std::string address;
    HostState state = HostState::Unknown;
    bool ps5 = false;
    std::string systemVersion;
    // chiaki's ChiakiTarget for this console's system version.
    int target = 0;
    // The address typed by the user whose direct query found it; empty when
    // the broadcast search did.
    std::string queriedHost;

    bool operator==(const DiscoveredHost&) const = default;
};

inline constexpr std::size_t kRegistKeySize = 16;
inline constexpr std::size_t kRpKeySize = 16;
inline constexpr std::size_t kMacSize = 6;

// What pairing gives this PC to wake a console and start a session on it.
struct Credentials
{
    std::array<std::uint8_t, kRegistKeySize> registKey{};
    std::array<std::uint8_t, kRpKeySize> rpKey{};
    std::uint32_t rpKeyType = 0;

    bool operator==(const Credentials&) const = default;
};

using Clock = std::chrono::system_clock;

// A console remembered on this PC: paired, renamed or added by its address.
struct SavedConsole
{
    // Empty for an address added by hand that hasn't answered yet.
    std::string hostId;
    // The name the console reported last.
    std::string hostName;
    // Chosen by the user; empty shows hostName.
    std::string nickname;
    // The address it last answered from.
    std::string address;
    // The address typed in "Adicionar pelo IP", queried directly.
    std::string manualHost;
    // The address set for playing away from home (a VPN or a port-forwarded
    // address), queried directly; empty when none is set.
    std::string remoteHost;
    bool ps5 = true;
    int target = 0;
    std::string systemVersion;
    std::array<std::uint8_t, kMacSize> serverMac{};
    // Absent until the console is paired.
    std::optional<Credentials> credentials;
    std::optional<Clock::time_point> lastPlayed;
    std::optional<Clock::time_point> lastSeen;

    bool operator==(const SavedConsole&) const = default;
};

} // namespace mfplay::remoteplay
