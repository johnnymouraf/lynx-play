#include "remoteplay/PsnPolicy.h"

namespace mfplay::remoteplay
{

namespace
{

// A failure the network or the console may recover from on its own.
bool isTransient(PsnFailureKind kind)
{
    switch (kind)
    {
    case PsnFailureKind::Network:
    case PsnFailureKind::Timeout:
    case PsnFailureKind::Unknown:
        return true;
    case PsnFailureKind::InvalidDevice:
    case PsnFailureKind::Canceled:
    case PsnFailureKind::ConsoleOffline:
    case PsnFailureKind::Rejected:
        return false;
    }

    return false;
}

} // namespace

PlayRoute playRouteFor(const PlayFacts& facts)
{
    if (facts.answersLocally)
    {
        return PlayRoute::Local;
    }

    const bool otherRouteExists = facts.hasRemoteAddress || facts.internetReady;

    if (!otherRouteExists)
    {
        return PlayRoute::Local;
    }

    if (!facts.waited)
    {
        return PlayRoute::WaitForLocal;
    }

    if (facts.answersRemotely)
    {
        return PlayRoute::Remote;
    }

    return facts.internetReady ? PlayRoute::Internet : PlayRoute::Local;
}

bool shouldRetryPsn(SessionEnd end, std::optional<PsnFailureKind> failure, bool recovering,
                    int retriesDone)
{
    if (!recovering || retriesDone >= kMaxPsnRetries)
    {
        return false;
    }

    switch (end)
    {
    case SessionEnd::Disconnected:
    case SessionEnd::Unknown:
    case SessionEnd::ConsoleNotFound:
        return true;
    case SessionEnd::PsnFailed:
        return failure.has_value() && isTransient(*failure);
    case SessionEnd::Stopped:
    case SessionEnd::EndedByConsole:
    case SessionEnd::ConnectionRefused:
    case SessionEnd::InUse:
    case SessionEnd::ConsoleCrashed:
    case SessionEnd::VersionMismatch:
    case SessionEnd::LoginPinRequired:
    case SessionEnd::VideoFailed:
        return false;
    }

    return false;
}

} // namespace mfplay::remoteplay
