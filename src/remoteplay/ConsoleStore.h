#pragma once

#include "remoteplay/AccountId.h"
#include "remoteplay/Console.h"
#include "remoteplay/SecretProtector.h"

#include <expected>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace mfplay::remoteplay
{

inline constexpr int kConsoleFileVersion = 1;

// What consoles.json holds.
struct ConsoleFile
{
    // The last account ID used to pair, offered again for the next pairing.
    std::optional<AccountId> accountId;
    std::vector<SavedConsole> consoles;

    bool operator==(const ConsoleFile&) const = default;
};

// Reads consoles.json. A secret that can't be decrypted (a file copied from
// another user or PC) is dropped, which leaves its console unpaired.
[[nodiscard]] std::expected<ConsoleFile, std::string> readConsoleFile(
    std::istream& input, const SecretProtector& protector);

// Writes consoles.json with the account ID and every key encrypted. Writes
// nothing when one of them can't be encrypted.
[[nodiscard]] std::expected<void, std::string> writeConsoleFile(
    const ConsoleFile& file, std::ostream& output, const SecretProtector& protector);

// consoles.json on disk.
class ConsoleStore
{
public:
    ConsoleStore(std::filesystem::path path, SecretProtector protector);

    // A missing file is an empty store. A corrupt or foreign one is renamed
    // aside as a backup and the store starts empty.
    [[nodiscard]] ConsoleFile load() const;

    // Replaces the file through a temporary one, so a failure leaves the old
    // file whole.
    [[nodiscard]] std::expected<void, std::string> save(const ConsoleFile& file) const;

private:
    void moveAside(const std::string& reason) const;

    std::filesystem::path path_;
    SecretProtector protector_;
};

} // namespace mfplay::remoteplay
