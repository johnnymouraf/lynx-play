#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace mfplay::core
{

// %LOCALAPPDATA%\MFPlay: logs and caches that belong to this machine.
[[nodiscard]] std::expected<std::filesystem::path, std::string> localDataDir();

// %APPDATA%\MFPlay: what the user keeps, such as the paired consoles.
[[nodiscard]] std::expected<std::filesystem::path, std::string> roamingDataDir();

// Copies the LynxPlay folders the app used before its rename into the MFPlay
// folders that don't exist yet, leaving the old ones untouched. Must run before
// anything writes to the MFPlay folders. Returns what failed, for the log.
[[nodiscard]] std::vector<std::string> adoptLegacyDataDirs();

} // namespace mfplay::core
