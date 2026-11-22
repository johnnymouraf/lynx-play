#pragma once

#include <filesystem>

namespace lynx::app
{

class SettingsController;

// Plays an H.264 or H.265 elementary stream file in a loop in the stream
// window, without the main window, until that window closes. It checks the
// decoder and renderer without a console. Returns the process exit code.
[[nodiscard]] int playFile(const std::filesystem::path& path, double framesPerSecond,
                           SettingsController& settings);

} // namespace lynx::app
