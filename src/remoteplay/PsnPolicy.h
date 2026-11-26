#pragma once

#include "remoteplay/PsnConnection.h"
#include "remoteplay/StreamSession.h"

#include <chrono>
#include <optional>

namespace mfplay::remoteplay
{

// How long a Play press waits for the console to answer on the local network
// before the session goes over the PSN.
inline constexpr std::chrono::seconds kLocalAnswerWait{5};

// The reconnections tried after a PSN connection drops, before the error.
inline constexpr int kMaxPsnRetries = 3;

enum class PlayRoute
{
    // The console answers here, or the PSN can't reach it: the local network,
    // as before.
    Local,
    // No answer yet, and the PSN could carry the session: wait for the local
    // network first.
    WaitForLocal,
    // No answer on the local network after the wait, and the PSN reaches it.
    Internet,
};

// Where a Play press goes. `answers`: the console answered the local search
// now. `waited`: the wait for that answer has run out. `internetReady`: the
// PSN reaches the console and a valid sign-in is on hand.
[[nodiscard]] PlayRoute playRouteFor(bool answers, bool waited, bool internetReady);

// Whether a PSN connection that ended is tried again. `recovering`: the
// connection had reached the console, or a reconnection is under way; a failed
// first attempt is never retried. `retriesDone`: the reconnections already
// tried for this drop.
[[nodiscard]] bool shouldRetryPsn(SessionEnd end, std::optional<PsnFailureKind> failure,
                                  bool recovering, int retriesDone);

} // namespace mfplay::remoteplay
