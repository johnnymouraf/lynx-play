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
    // The console answers here, or no other route can reach it: the local
    // network, as before.
    Local,
    // No answer yet, and another route could carry the session: wait for the
    // local network first.
    WaitForLocal,
    // No answer on the local network after the wait, and the remote address
    // the user saved answers: over the internet, straight to it.
    Remote,
    // No answer on the local network after the wait, and the PSN reaches it.
    Internet,
};

// What a Play press knows about the console and the routes it can take.
struct PlayFacts
{
    // The console answered the local network search now.
    bool answersLocally = false;
    // The wait for the local network has run out.
    bool waited = false;
    // A remote address is saved for the console.
    bool hasRemoteAddress = false;
    // The remote address answered a direct query now.
    bool answersRemotely = false;
    // The PSN reaches the console and a valid sign-in is on hand.
    bool internetReady = false;
};

// Where a Play press goes. The local network always gets the first answer; a
// remote address comes before the PSN.
[[nodiscard]] PlayRoute playRouteFor(const PlayFacts& facts);

// Whether a PSN connection that ended is tried again. `recovering`: the
// connection had reached the console, or a reconnection is under way; a failed
// first attempt is never retried. `retriesDone`: the reconnections already
// tried for this drop.
[[nodiscard]] bool shouldRetryPsn(SessionEnd end, std::optional<PsnFailureKind> failure,
                                  bool recovering, int retriesDone);

} // namespace mfplay::remoteplay
