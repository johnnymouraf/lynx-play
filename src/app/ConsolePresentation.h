#pragma once

#include "remoteplay/Console.h"

#include <QDateTime>
#include <QString>

namespace lynx::app
{

// What a card needs to know about one console.
struct ConsoleFacts
{
    const remoteplay::SavedConsole* saved = nullptr;
    const remoteplay::DiscoveredHost* discovered = nullptr;
    bool waking = false;
    bool searching = false;
};

// The card's state: ready, standby, unpaired, offline or waking.
[[nodiscard]] QString consoleStatus(const ConsoleFacts& facts);

// The card's last line: when it was played, found or last seen.
[[nodiscard]] QString consoleActivity(const ConsoleFacts& facts, const QDateTime& now);

// The nickname, else the console's own name, else its address.
[[nodiscard]] QString consoleName(const ConsoleFacts& facts);

[[nodiscard]] QString consoleAddress(const ConsoleFacts& facts);

[[nodiscard]] QString fromUtf8(const std::string& text);

} // namespace lynx::app
