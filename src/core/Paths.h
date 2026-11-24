#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace mfplay::core
{

// %LOCALAPPDATA%\MFPlay: logs and caches that belong to this machine.
[[nodiscard]] std::expected<std::filesystem::path, std::string> localDataDir();

// %APPDATA%\MFPlay: what the user keeps, such as the paired consoles.
[[nodiscard]] std::expected<std::filesystem::path, std::string> roamingDataDir();

} // namespace mfplay::core
