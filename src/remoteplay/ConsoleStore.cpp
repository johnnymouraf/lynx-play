#include "remoteplay/ConsoleStore.h"

#include "core/Files.h"
#include "core/Text.h"
#include "remoteplay/Base64.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <format>
#include <fstream>
#include <istream>
#include <ostream>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace mfplay::remoteplay
{

namespace
{

using Json = nlohmann::ordered_json;

constexpr int kJsonIndent = 2;
constexpr int kHexBase = 16;

std::array<std::uint8_t, kMacSize> macFromHex(std::string_view text)
{
    if (text.size() != 2 * kMacSize)
    {
        return {};
    }

    std::array<std::uint8_t, kMacSize> mac{};

    for (std::size_t i = 0; i < kMacSize; ++i)
    {
        const char* first = text.data() + 2 * i;
        const auto result = std::from_chars(first, first + 2, mac[i], kHexBase);

        if (result.ec != std::errc())
        {
            return {};
        }
    }

    return mac;
}

std::int64_t toSeconds(Clock::time_point time)
{
    return std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
}

Clock::time_point fromSeconds(std::int64_t seconds)
{
    return Clock::time_point(std::chrono::seconds(seconds));
}

std::expected<std::string, std::string> sealSecret(std::span<const std::uint8_t> secret,
                                                   const SecretProtector& protector)
{
    const auto sealed = protector.protect(secret);

    if (!sealed)
    {
        return std::unexpected(sealed.error());
    }

    return encodeBase64(*sealed);
}

template <std::size_t Size>
std::expected<std::array<std::uint8_t, Size>, std::string> openSecret(
    const std::string& text, const SecretProtector& protector)
{
    const auto sealed = decodeBase64(text);

    if (!sealed)
    {
        return std::unexpected(std::string("not Base64"));
    }

    const auto plain = protector.unprotect(*sealed);

    if (!plain)
    {
        return std::unexpected(plain.error());
    }

    if (plain->size() != Size)
    {
        return std::unexpected(std::string("wrong size"));
    }

    std::array<std::uint8_t, Size> secret{};
    std::ranges::copy(*plain, secret.begin());
    return secret;
}

std::expected<Json, std::string> consoleToJson(const SavedConsole& console,
                                               const SecretProtector& protector)
{
    Json entry{
        {"hostId", console.hostId},
        {"hostName", console.hostName},
        {"nickname", console.nickname},
        {"address", console.address},
        {"manualHost", console.manualHost},
        {"ps5", console.ps5},
        {"target", console.target},
        {"systemVersion", console.systemVersion},
        {"serverMac", core::toHex(console.serverMac)},
    };

    if (console.credentials)
    {
        const auto registKey = sealSecret(console.credentials->registKey, protector);

        if (!registKey)
        {
            return std::unexpected(registKey.error());
        }

        const auto rpKey = sealSecret(console.credentials->rpKey, protector);

        if (!rpKey)
        {
            return std::unexpected(rpKey.error());
        }

        entry["registKey"] = *registKey;
        entry["rpKey"] = *rpKey;
        entry["rpKeyType"] = console.credentials->rpKeyType;
    }

    if (console.lastPlayed)
    {
        entry["lastPlayed"] = toSeconds(*console.lastPlayed);
    }

    if (console.lastSeen)
    {
        entry["lastSeen"] = toSeconds(*console.lastSeen);
    }

    return entry;
}

std::optional<Credentials> credentialsFromJson(const Json& entry, const SecretProtector& protector,
                                               const std::string& hostId)
{
    if (!entry.contains("registKey") || !entry.contains("rpKey"))
    {
        return std::nullopt;
    }

    const auto registKey =
        openSecret<kRegistKeySize>(entry.at("registKey").get<std::string>(), protector);
    const auto rpKey = openSecret<kRpKeySize>(entry.at("rpKey").get<std::string>(), protector);

    if (!registKey || !rpKey)
    {
        const std::string& reason = !registKey ? registKey.error() : rpKey.error();
        spdlog::warn("Console {} is no longer paired: its keys can't be read ({})", hostId, reason);
        return std::nullopt;
    }

    return Credentials{
        .registKey = *registKey,
        .rpKey = *rpKey,
        .rpKeyType = entry.value("rpKeyType", std::uint32_t{0}),
    };
}

SavedConsole consoleFromJson(const Json& entry, const SecretProtector& protector)
{
    SavedConsole console{
        .hostId = entry.value("hostId", std::string()),
        .hostName = entry.value("hostName", std::string()),
        .nickname = entry.value("nickname", std::string()),
        .address = entry.value("address", std::string()),
        .manualHost = entry.value("manualHost", std::string()),
        .ps5 = entry.value("ps5", true),
        .target = entry.value("target", 0),
        .systemVersion = entry.value("systemVersion", std::string()),
        .serverMac = macFromHex(entry.value("serverMac", std::string())),
        .credentials = std::nullopt,
        .lastPlayed = std::nullopt,
        .lastSeen = std::nullopt,
    };

    console.credentials = credentialsFromJson(entry, protector, console.hostId);

    if (entry.contains("lastPlayed"))
    {
        console.lastPlayed = fromSeconds(entry.at("lastPlayed").get<std::int64_t>());
    }

    if (entry.contains("lastSeen"))
    {
        console.lastSeen = fromSeconds(entry.at("lastSeen").get<std::int64_t>());
    }

    return console;
}

std::expected<ConsoleFile, std::string> fileFromJson(const Json& root,
                                                     const SecretProtector& protector)
{
    if (!root.is_object() || !root.contains("version") || !root.contains("consoles"))
    {
        return std::unexpected(std::string("not a MFPlay console list"));
    }

    const int version = root.at("version").get<int>();

    if (version < 1 || version > kConsoleFileVersion)
    {
        return std::unexpected(std::format("unsupported version {}", version));
    }

    ConsoleFile file;

    for (const Json& entry : root.at("consoles"))
    {
        file.consoles.push_back(consoleFromJson(entry, protector));
    }

    if (root.contains("accountId"))
    {
        const auto accountId =
            openSecret<kAccountIdSize>(root.at("accountId").get<std::string>(), protector);

        if (accountId)
        {
            file.accountId = *accountId;
        }
    }

    return file;
}

} // namespace

std::expected<ConsoleFile, std::string> readConsoleFile(std::istream& input,
                                                        const SecretProtector& protector)
{
    const Json root = Json::parse(input, nullptr, false);

    if (root.is_discarded())
    {
        return std::unexpected(std::string("not valid JSON"));
    }

    try
    {
        return fileFromJson(root, protector);
    }
    catch (const Json::exception& exception)
    {
        return std::unexpected(std::string("unexpected content: ") + exception.what());
    }
}

std::expected<void, std::string> writeConsoleFile(const ConsoleFile& file, std::ostream& output,
                                                  const SecretProtector& protector)
{
    Json root{
        {"version", kConsoleFileVersion},
    };

    if (file.accountId)
    {
        const auto accountId = sealSecret(*file.accountId, protector);

        if (!accountId)
        {
            return std::unexpected(accountId.error());
        }

        root["accountId"] = *accountId;
    }

    Json consoles = Json::array();

    for (const SavedConsole& console : file.consoles)
    {
        auto entry = consoleToJson(console, protector);

        if (!entry)
        {
            return std::unexpected(entry.error());
        }

        consoles.push_back(std::move(*entry));
    }

    root["consoles"] = std::move(consoles);
    output << root.dump(kJsonIndent) << '\n';

    if (!output)
    {
        return std::unexpected(std::string("cannot write the console list"));
    }

    return {};
}

ConsoleStore::ConsoleStore(std::filesystem::path path, SecretProtector protector)
    : path_(std::move(path)), protector_(std::move(protector))
{
}

ConsoleFile ConsoleStore::load() const
{
    std::error_code error;

    if (!std::filesystem::exists(path_, error))
    {
        return {};
    }

    std::ifstream input(path_, std::ios::binary);

    if (!input)
    {
        spdlog::error("Cannot open {}", path_.string());
        return {};
    }

    auto file = readConsoleFile(input, protector_);
    input.close();

    if (!file)
    {
        moveAside(file.error());
        return {};
    }

    spdlog::info("Loaded {} saved console(s)", file->consoles.size());
    return std::move(*file);
}

std::expected<void, std::string> ConsoleStore::save(const ConsoleFile& file) const
{
    std::ostringstream buffer;

    if (auto written = writeConsoleFile(file, buffer, protector_); !written)
    {
        return std::unexpected(written.error());
    }

    return core::replaceFile(path_, buffer.str());
}

void ConsoleStore::moveAside(const std::string& reason) const
{
    const auto now = std::chrono::floor<std::chrono::seconds>(Clock::now());
    std::filesystem::path backup = path_;
    backup += std::format(".{:%Y%m%d-%H%M%S}.bak", now);

    std::error_code error;
    std::filesystem::rename(path_, backup, error);

    if (error)
    {
        spdlog::error("{} is unreadable ({}) and can't be moved aside: {}", path_.string(), reason,
                      error.message());
        return;
    }

    spdlog::error("{} is unreadable ({}); kept as {}, starting with no saved consoles",
                  path_.string(), reason, backup.string());
}

} // namespace mfplay::remoteplay
