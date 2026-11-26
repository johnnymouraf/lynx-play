#include "core/PathsAndroid.h"

#include "core/Paths.h"

#include <utility>

namespace mfplay::core
{

namespace
{

std::filesystem::path privateDataDir;

std::expected<std::filesystem::path, std::string> appDataDir()
{
    if (privateDataDir.empty())
    {
        return std::unexpected(std::string("the app's private folder was not given"));
    }

    return privateDataDir;
}

} // namespace

void setPrivateDataDir(std::filesystem::path dir)
{
    privateDataDir = std::move(dir);
}

std::expected<std::filesystem::path, std::string> localDataDir()
{
    return appDataDir();
}

std::expected<std::filesystem::path, std::string> roamingDataDir()
{
    return appDataDir();
}

} // namespace mfplay::core
