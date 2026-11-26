#include "remoteplay/PsnPolicy.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>

using namespace mfplay::remoteplay;

TEST_CASE("A console that answers on the local network is played there")
{
    CHECK(playRouteFor(true, false, true) == PlayRoute::Local);
    CHECK(playRouteFor(true, true, true) == PlayRoute::Local);
    CHECK(playRouteFor(true, false, false) == PlayRoute::Local);
}

TEST_CASE("Without a PSN route the local network is tried at once")
{
    CHECK(playRouteFor(false, false, false) == PlayRoute::Local);
    CHECK(playRouteFor(false, true, false) == PlayRoute::Local);
}

TEST_CASE("A console that doesn't answer is waited for before the internet")
{
    CHECK(playRouteFor(false, false, true) == PlayRoute::WaitForLocal);
}

TEST_CASE("The internet takes over once the local wait has run out")
{
    CHECK(playRouteFor(false, true, true) == PlayRoute::Internet);
}

TEST_CASE("A session the user stops is never retried")
{
    CHECK_FALSE(shouldRetryPsn(SessionEnd::Stopped, std::nullopt, true, 0));
}

TEST_CASE("A console that ends the stream on its side is not a drop")
{
    CHECK_FALSE(shouldRetryPsn(SessionEnd::EndedByConsole, std::nullopt, true, 0));
}

TEST_CASE("A connection lost after it was up is retried")
{
    CHECK(shouldRetryPsn(SessionEnd::Disconnected, std::nullopt, true, 0));
    CHECK(shouldRetryPsn(SessionEnd::Unknown, std::nullopt, true, 0));
    CHECK(shouldRetryPsn(SessionEnd::ConsoleNotFound, std::nullopt, true, 0));
}

TEST_CASE("A first attempt that never reached the console is not retried")
{
    CHECK_FALSE(shouldRetryPsn(SessionEnd::Disconnected, std::nullopt, false, 0));
    CHECK_FALSE(shouldRetryPsn(SessionEnd::PsnFailed, PsnFailureKind::Network, false, 0));
}

TEST_CASE("A failed reconnection is retried while the network may recover")
{
    CHECK(shouldRetryPsn(SessionEnd::PsnFailed, PsnFailureKind::Network, true, 1));
    CHECK(shouldRetryPsn(SessionEnd::PsnFailed, PsnFailureKind::Timeout, true, 1));
    CHECK(shouldRetryPsn(SessionEnd::PsnFailed, PsnFailureKind::Unknown, true, 1));
}

TEST_CASE("A refused login or a console that is off is not retried")
{
    CHECK_FALSE(shouldRetryPsn(SessionEnd::PsnFailed, PsnFailureKind::Rejected, true, 0));
    CHECK_FALSE(shouldRetryPsn(SessionEnd::PsnFailed, PsnFailureKind::ConsoleOffline, true, 0));
    CHECK_FALSE(shouldRetryPsn(SessionEnd::PsnFailed, PsnFailureKind::InvalidDevice, true, 0));
    CHECK_FALSE(shouldRetryPsn(SessionEnd::PsnFailed, PsnFailureKind::Canceled, true, 0));
}

TEST_CASE("Reconnections stop after three tries for one drop")
{
    CHECK(shouldRetryPsn(SessionEnd::Disconnected, std::nullopt, true, 2));
    CHECK_FALSE(shouldRetryPsn(SessionEnd::Disconnected, std::nullopt, true, 3));
    CHECK_FALSE(shouldRetryPsn(SessionEnd::PsnFailed, PsnFailureKind::Network, true, 3));
}

TEST_CASE("Ends that a retry can't fix are not retried")
{
    CHECK_FALSE(shouldRetryPsn(SessionEnd::InUse, std::nullopt, true, 0));
    CHECK_FALSE(shouldRetryPsn(SessionEnd::LoginPinRequired, std::nullopt, true, 0));
    CHECK_FALSE(shouldRetryPsn(SessionEnd::VideoFailed, std::nullopt, true, 0));
    CHECK_FALSE(shouldRetryPsn(SessionEnd::VersionMismatch, std::nullopt, true, 0));
}
