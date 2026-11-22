#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace lynx::core
{

// %LOCALAPPDATA%\LynxPlay: logs and caches that belong to this machine.
[[nodiscard]] std::expected<std::filesystem::path, std::string> localDataDir();

// %APPDATA%\LynxPlay: what the user keeps, such as the paired consoles.
[[nodiscard]] std::expected<std::filesystem::path, std::string> roamingDataDir();

} // namespace lynx::core
