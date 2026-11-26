#include "core/Paths.h"

namespace mfplay::core
{

std::expected<std::filesystem::path, std::string> localDataDir()
{
    return std::unexpected(std::string("app folders are not implemented on this platform yet"));
}

std::expected<std::filesystem::path, std::string> roamingDataDir()
{
    return std::unexpected(std::string("app folders are not implemented on this platform yet"));
}

} // namespace mfplay::core
