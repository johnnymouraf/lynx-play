#pragma once

#include "core/ReflexMode.h"
#include "render/SwapChain.h"

#include <filesystem>

namespace lynx::app
{

// Plays an H.264 or H.265 elementary stream file in a loop in the stream
// window, without the main window, until that window closes. It checks the
// decoder and renderer without a console. Returns the process exit code.
[[nodiscard]] int playFile(const std::filesystem::path& path, double framesPerSecond,
                           render::PresentTiming timing, core::ReflexMode reflex);

} // namespace lynx::app
