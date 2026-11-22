#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace lynx::app
{

// Registers the fonts bundled as resources (Sora, Inter and JetBrains Mono)
// and makes Inter the application's default font.
void loadBundledFonts();

// The JetBrains Mono bytes for the stream overlay's Dear ImGui font; empty
// when the resource is missing.
[[nodiscard]] std::shared_ptr<const std::vector<std::uint8_t>> overlayFont();

} // namespace lynx::app
