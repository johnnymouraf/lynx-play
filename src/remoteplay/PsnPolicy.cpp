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

PlayRoute playRouteFor(bool answers, bool waited, bool internetReady)
{
    if (answers || !internetReady)
    {
        return PlayRoute::Local;
    }

    return waited ? PlayRoute::Internet : PlayRoute::WaitForLocal;
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
