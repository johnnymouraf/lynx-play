#include "core/Windows.h"

#include <format>

namespace mfplay::core
{

std::string toUtf8(std::wstring_view text)
{
    if (text.empty())
    {
        return {};
    }

    const auto wideLength = static_cast<int>(text.size());
    const int utf8Length =
        WideCharToMultiByte(CP_UTF8, 0, text.data(), wideLength, nullptr, 0, nullptr, nullptr);

    std::string utf8(static_cast<std::size_t>(utf8Length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), wideLength, utf8.data(), utf8Length, nullptr,
                        nullptr);

    return utf8;
}

std::string describeFailure(std::string_view what, HRESULT result)
{
    return std::format("{} failed: 0x{:08X}", what, static_cast<unsigned long>(result));
}

} // namespace mfplay::core
