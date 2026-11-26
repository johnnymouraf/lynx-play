#pragma once

#include "remoteplay/ConsoleRegistration.h"

#include <chiaki/regist.h>

namespace mfplay::remoteplay
{

// chiaki's registered host as the app keeps it: the keys, the console's MAC,
// nickname and target. The LAN pairing and the registration over the PSN both
// get their result from it.
[[nodiscard]] PairedConsole pairedConsoleOf(const ChiakiRegisteredHost& host);

} // namespace mfplay::remoteplay
