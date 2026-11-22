#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace lynx::core
{

// %LOCALAPPDATA%\LynxPlay: logs and caches that belong to this machine.
[[nodiscard]] std::expected<std::filesystem::path, std::string> localDataDir();

} // namespace lynx::core
