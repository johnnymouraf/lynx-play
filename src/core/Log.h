#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace mfplay::core
{

// Makes spdlog's default logger write to `<directory>/mfplay.log` (rotated)
// and to the debugger output. Until it succeeds, logs only reach stderr.
[[nodiscard]] std::expected<void, std::string> initLogging(const std::filesystem::path& directory);

} // namespace mfplay::core
