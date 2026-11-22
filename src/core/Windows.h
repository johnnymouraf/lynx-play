#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <windows.h>

namespace lynx::core
{

struct HandleCloser
{
    void operator()(HANDLE handle) const noexcept
    {
        CloseHandle(handle);
    }
};

// Owns a kernel handle (event, waitable object) and closes it.
using UniqueHandle = std::unique_ptr<void, HandleCloser>;

[[nodiscard]] std::string toUtf8(std::wstring_view text);

// "<what> failed: 0x887A0005", for errors that come back as an HRESULT.
[[nodiscard]] std::string describeFailure(std::string_view what, HRESULT result);

} // namespace lynx::core
