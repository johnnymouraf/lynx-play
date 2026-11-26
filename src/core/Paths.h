#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace mfplay::core
{

// Windows: %LOCALAPPDATA%\MFPlay, for logs and caches that belong to this machine.
[[nodiscard]] std::expected<std::filesystem::path, std::string> localDataDir();

// Windows: %APPDATA%\MFPlay, for what the user keeps, such as the paired consoles.
[[nodiscard]] std::expected<std::filesystem::path, std::string> roamingDataDir();

} // namespace mfplay::core
