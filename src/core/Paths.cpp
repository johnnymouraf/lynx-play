#include "core/Paths.h"

#include <format>
#include <memory>

#include <windows.h>

#include <shlobj.h>

namespace lynx::core
{

namespace
{

constexpr auto kAppFolderName = L"LynxPlay";

struct CoTaskMemDeleter
{
    void operator()(wchar_t* memory) const noexcept
    {
        CoTaskMemFree(memory);
    }
};

} // namespace

std::expected<std::filesystem::path, std::string> localDataDir()
{
    wchar_t* rawPath = nullptr;
    const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr,
                                                &rawPath);
    const std::unique_ptr<wchar_t, CoTaskMemDeleter> knownFolder(rawPath);

    if (FAILED(result))
    {
        return std::unexpected(
            std::format("SHGetKnownFolderPath failed: 0x{:08X}", static_cast<unsigned long>(result)));
    }

    return std::filesystem::path(knownFolder.get()) / kAppFolderName;
}

} // namespace lynx::core
