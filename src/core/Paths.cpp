#include "core/Paths.h"

#include "core/Windows.h"

#include <format>
#include <memory>

#include <shlobj.h>

namespace mfplay::core
{

namespace
{

constexpr auto kAppFolderName = L"MFPlay";
constexpr auto kLegacyAppFolderName = L"LynxPlay";
constexpr auto kPartialCopySuffix = L".partial";

struct CoTaskMemDeleter
{
    void operator()(wchar_t* memory) const noexcept
    {
        CoTaskMemFree(memory);
    }
};

std::expected<std::filesystem::path, std::string> knownFolder(const KNOWNFOLDERID& knownFolderId)
{
    wchar_t* rawPath = nullptr;
    const HRESULT result = SHGetKnownFolderPath(knownFolderId, KF_FLAG_DEFAULT, nullptr, &rawPath);
    const std::unique_ptr<wchar_t, CoTaskMemDeleter> folder(rawPath);

    if (FAILED(result))
    {
        return std::unexpected(describeFailure("SHGetKnownFolderPath", result));
    }

    return std::filesystem::path(folder.get());
}

std::expected<std::filesystem::path, std::string> appFolderIn(const KNOWNFOLDERID& knownFolderId)
{
    return knownFolder(knownFolderId).transform(
        [](const std::filesystem::path& folder) { return folder / kAppFolderName; });
}

std::string copyFailure(const std::filesystem::path& from, const std::error_code& error)
{
    return std::format("copying {}: {}", from.string(), error.message());
}

// The copy lands in a sibling folder first and is renamed at the end, so an
// interrupted copy is redone on the next run instead of leaving half the data.
std::expected<void, std::string> adoptLegacyFolderIn(const KNOWNFOLDERID& knownFolderId)
{
    const auto base = knownFolder(knownFolderId);

    if (!base)
    {
        return std::unexpected(base.error());
    }

    const auto legacy = *base / kLegacyAppFolderName;
    const auto current = *base / kAppFolderName;
    std::error_code error;

    const bool alreadyAdopted = std::filesystem::exists(current, error);
    const bool nothingToAdopt = !std::filesystem::is_directory(legacy, error);

    if (alreadyAdopted || nothingToAdopt)
    {
        return {};
    }

    auto partial = current;
    partial += kPartialCopySuffix;
    std::filesystem::remove_all(partial, error);
    std::filesystem::copy(legacy, partial, std::filesystem::copy_options::recursive, error);

    if (error)
    {
        return std::unexpected(copyFailure(legacy, error));
    }

    std::filesystem::rename(partial, current, error);

    if (error)
    {
        return std::unexpected(copyFailure(legacy, error));
    }

    return {};
}

} // namespace

std::expected<std::filesystem::path, std::string> localDataDir()
{
    return appFolderIn(FOLDERID_LocalAppData);
}

std::expected<std::filesystem::path, std::string> roamingDataDir()
{
    return appFolderIn(FOLDERID_RoamingAppData);
}

std::vector<std::string> adoptLegacyDataDirs()
{
    std::vector<std::string> failures;

    for (const auto& knownFolderId : {FOLDERID_RoamingAppData, FOLDERID_LocalAppData})
    {
        if (const auto adopted = adoptLegacyFolderIn(knownFolderId); !adopted)
        {
            failures.push_back(adopted.error());
        }
    }

    return failures;
}

} // namespace mfplay::core
