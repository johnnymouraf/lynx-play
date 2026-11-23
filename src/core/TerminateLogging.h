#pragma once

namespace lynx::core
{

// Installs a std::terminate handler that logs the active exception (its type
// and what()) and the current thread's call stack before the previous handler
// ends the process. Call once, after logging is initialized.
void installTerminateLogging();

// Logs the exception being handled in this catch block, with its type, its
// message and the current thread's stack, for a handler that rethrows.
void logUnhandledException(const char* context);

} // namespace lynx::core
