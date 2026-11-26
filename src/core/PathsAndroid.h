#pragma once

#include <filesystem>

namespace mfplay::core
{

// Android has no environment folders for the app. The application layer passes
// its private files folder once, before the first call to localDataDir or
// roamingDataDir; both folders live in it.
void setPrivateDataDir(std::filesystem::path dir);

} // namespace mfplay::core
