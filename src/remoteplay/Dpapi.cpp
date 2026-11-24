#include "remoteplay/SecretProtector.h"

#include "core/Windows.h"

#include <wincrypt.h>

#include <dpapi.h>

#include <memory>
#include <string_view>

namespace mfplay::remoteplay
{

namespace
{

// Fixed when the app was still called Lynx Play: changing it makes every saved
// registration key and PSN token unreadable.
constexpr std::string_view kEntropy = "Lynx Play console secrets";

struct LocalFreeDeleter
{
    void operator()(BYTE* memory) const noexcept
    {
        LocalFree(memory);
    }
};

// DPAPI takes non-const blobs but never writes to its inputs.
DATA_BLOB blobOf(std::span<const std::uint8_t> bytes)
{
    return DATA_BLOB{
        .cbData = static_cast<DWORD>(bytes.size()),
        .pbData = const_cast<BYTE*>(bytes.data()),
    };
}

DATA_BLOB entropyBlob()
{
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(kEntropy.data());
    return blobOf(std::span(bytes, kEntropy.size()));
}

SecretBytes takeOutput(DATA_BLOB output)
{
    const std::unique_ptr<BYTE, LocalFreeDeleter> owned(output.pbData);
    const std::span<const std::uint8_t> bytes(output.pbData, output.cbData);
    SecretBytes copy(bytes.begin(), bytes.end());
    SecureZeroMemory(output.pbData, output.cbData);

    return copy;
}

std::string lastErrorAs(std::string_view what)
{
    return core::describeFailure(what, HRESULT_FROM_WIN32(GetLastError()));
}

SecretResult protectWithDpapi(std::span<const std::uint8_t> plain)
{
    DATA_BLOB input = blobOf(plain);
    DATA_BLOB entropy = entropyBlob();
    DATA_BLOB output{};

    if (!CryptProtectData(&input, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                          &output))
    {
        return std::unexpected(lastErrorAs("CryptProtectData"));
    }

    return takeOutput(output);
}

SecretResult unprotectWithDpapi(std::span<const std::uint8_t> sealed)
{
    DATA_BLOB input = blobOf(sealed);
    DATA_BLOB entropy = entropyBlob();
    DATA_BLOB output{};

    if (!CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &output))
    {
        return std::unexpected(lastErrorAs("CryptUnprotectData"));
    }

    return takeOutput(output);
}

} // namespace

SecretProtector dpapiProtector()
{
    return SecretProtector{
        .protect = protectWithDpapi,
        .unprotect = unprotectWithDpapi,
    };
}

} // namespace mfplay::remoteplay
