#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace mfplay::core
{

// Writes the file through a temporary one next to it, creating its folder
// when needed, so a failure leaves the old file whole.
[[nodiscard]] std::expected<void, std::string> replaceFile(const std::filesystem::path& path,
                                                           std::string_view content);

} // namespace mfplay::core
