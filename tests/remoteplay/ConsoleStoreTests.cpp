#include "remoteplay/Base64.h"
#include "remoteplay/ConsoleStore.h"

#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace mfplay::remoteplay;

namespace
{

// Obviously fake keys; real ones never go into the repository.
Credentials fakeCredentials()
{
    Credentials credentials;
    credentials.registKey = {'1', '2', '3', '4', 'a', 'b', 'c', 'd'};
    credentials.rpKey = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                         0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    credentials.rpKeyType = 2;
    return credentials;
}

ConsoleFile sampleFile()
{
    SavedConsole paired;
    paired.hostId = "001122334455";
    paired.hostName = "PS5-123";
    paired.nickname = "PS5 da sala";
    paired.address = "192.168.0.25";
    paired.ps5 = true;
    paired.target = 1000100;
    paired.systemVersion = "09100000";
    paired.serverMac = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    paired.credentials = fakeCredentials();
    paired.lastPlayed = Clock::time_point(std::chrono::seconds(1'790'000'000));
    paired.lastSeen = Clock::time_point(std::chrono::seconds(1'790'000'500));

    SavedConsole manual;
    manual.manualHost = "ps5.local";

    ConsoleFile file;
    file.accountId = AccountId{1, 2, 3, 4, 5, 6, 7, 8};
    file.consoles = {paired, manual};
    return file;
}

SecretResult refuse(std::span<const std::uint8_t>)
{
    return std::unexpected(std::string("refused"));
}

SecretProtector refusingProtector()
{
    return SecretProtector{.protect = refuse, .unprotect = refuse};
}

bool canRead(const std::string& text)
{
    std::istringstream stream(text);
    return readConsoleFile(stream, dpapiProtector()).has_value();
}

} // namespace

TEST_CASE("The console store gives back every console it saved")
{
    const ConsoleFile file = sampleFile();
    const SecretProtector protector = dpapiProtector();
    std::stringstream stream;

    REQUIRE(writeConsoleFile(file, stream, protector));
    const auto loaded = readConsoleFile(stream, protector);

    REQUIRE(loaded);
    CHECK(*loaded == file);
}

TEST_CASE("The console store keeps the keys and the account ID encrypted on disk")
{
    const ConsoleFile file = sampleFile();
    std::stringstream stream;

    REQUIRE(writeConsoleFile(file, stream, dpapiProtector()));
    const std::string text = stream.str();
    const Credentials credentials = fakeCredentials();

    CHECK(text.find("\"registKey\"") != std::string::npos);
    CHECK(text.find("1234abcd") == std::string::npos);
    CHECK(text.find(encodeBase64(credentials.registKey)) == std::string::npos);
    CHECK(text.find(encodeBase64(credentials.rpKey)) == std::string::npos);
    CHECK(text.find(encodeBase64(*file.accountId)) == std::string::npos);
}

TEST_CASE("The console store writes nothing when a secret can't be encrypted")
{
    std::stringstream stream;

    CHECK_FALSE(writeConsoleFile(sampleFile(), stream, refusingProtector()));
    CHECK(stream.str().empty());
}

TEST_CASE("A console whose keys can't be decrypted loads unpaired")
{
    std::stringstream stream;
    REQUIRE(writeConsoleFile(sampleFile(), stream, dpapiProtector()));

    const auto loaded = readConsoleFile(stream, refusingProtector());

    REQUIRE(loaded);
    REQUIRE(loaded->consoles.size() == 2);
    CHECK(loaded->consoles.front().hostId == "001122334455");
    CHECK_FALSE(loaded->consoles.front().credentials.has_value());
    CHECK_FALSE(loaded->accountId.has_value());
}

TEST_CASE("The console store rejects a corrupt or foreign file")
{
    CHECK_FALSE(canRead("not json"));
    CHECK_FALSE(canRead("{\"name\": \"another app\"}"));
    CHECK_FALSE(canRead("{\"version\": 99, \"consoles\": []}"));
    CHECK_FALSE(canRead("{\"version\": 1, \"consoles\": [{\"hostId\": 5}]}"));
    CHECK(canRead("{\"version\": 1, \"consoles\": []}"));
}
