#include "remoteplay/PsnPolicy.h"

#include <catch2/catch_test_macros.hpp>

#include <optional>

using namespace mfplay::remoteplay;

TEST_CASE("A console that answers on the local network is played there")
{
    CHECK(playRouteFor({.answersLocally = true}) == PlayRoute::Local);
    CHECK(playRouteFor({.answersLocally = true, .waited = true, .internetReady = true}) ==
          PlayRoute::Local);
    CHECK(playRouteFor({.answersLocally = true,
                        .hasRemoteAddress = true,
                        .answersRemotely = true,
                        .waited = true,
                        .internetReady = true}) == PlayRoute::Local);
}

TEST_CASE("Without another route the local network is tried at once")
{
    CHECK(playRouteFor({}) == PlayRoute::Local);
    CHECK(playRouteFor({.waited = true}) == PlayRoute::Local);
}

TEST_CASE("A console that doesn't answer is waited for before the internet")
{
    CHECK(playRouteFor({.internetReady = true}) == PlayRoute::WaitForLocal);
}

TEST_CASE("The internet takes over once the local wait has run out")
{
    CHECK(playRouteFor({.waited = true, .internetReady = true}) == PlayRoute::Internet);
}

TEST_CASE("A saved remote address is waited for, without a PSN route")
{
    CHECK(playRouteFor({.hasRemoteAddress = true}) == PlayRoute::WaitForLocal);
}

TEST_CASE("A remote answer before the local wait ends still waits for the local network")
{
    CHECK(playRouteFor({.hasRemoteAddress = true,
                        .answersRemotely = true,
                        .internetReady = true}) == PlayRoute::WaitForLocal);
}

TEST_CASE("A remote answer after the local wait is played over the internet")
{
    CHECK(playRouteFor({.hasRemoteAddress = true,
                        .answersRemotely = true,
                        .waited = true}) == PlayRoute::Remote);
    CHECK(playRouteFor({.hasRemoteAddress = true,
                        .answersRemotely = true,
                        .waited = true,
                        .internetReady = true}) == PlayRoute::Remote);
}

TEST_CASE("A silent remote address gives way to the PSN, and then to the local network")
{
    CHECK(playRouteFor({.hasRemoteAddress = true, .waited = true, .internetReady = true}) ==
          PlayRoute::Internet);
    CHECK(playRouteFor({.hasRemoteAddress = true, .waited = true}) == PlayRoute::Local);
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
