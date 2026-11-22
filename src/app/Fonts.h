#pragma once

#include "render/Overlay.h"

namespace lynx::app
{

// Registers the fonts bundled as resources (Sora, Inter and JetBrains Mono)
// and makes Inter the application's default font.
void loadBundledFonts();

// The Inter and JetBrains Mono bytes for the stream overlay's Dear ImGui
// fonts; empty when a resource is missing.
[[nodiscard]] render::OverlayFonts overlayFonts();

} // namespace lynx::app
