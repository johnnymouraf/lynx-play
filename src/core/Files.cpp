#include "core/Files.h"

#include <fstream>

namespace lynx::core
{

std::expected<void, std::string> replaceFile(const std::filesystem::path& path,
                                             std::string_view content)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);

    if (error)
    {
        return std::unexpected("cannot create its folder: " + error.message());
    }

    std::filesystem::path temporary = path;
    temporary += ".tmp";

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << content;

        if (!output.flush())
        {
            return std::unexpected("cannot write " + temporary.string());
        }
    }

    std::filesystem::rename(temporary, path, error);

    if (error)
    {
        return std::unexpected("cannot replace the file: " + error.message());
    }

    return {};
}

} // namespace lynx::core
