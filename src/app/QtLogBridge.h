#pragma once

namespace lynx::app
{

// Sends Qt's own messages (QML errors, plugin warnings) to spdlog's default
// logger, so they land in the log file of a GUI app that has no console.
void installQtLogBridge();

} // namespace lynx::app
