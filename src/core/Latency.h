#pragma once

#include <string_view>

#include <windows.h>

namespace lynx::core
{

// Registers the current thread with MMCSS (Windows' multimedia scheduler)
// and unregisters it on destruction. Logs whether it worked.
class MmcssRegistration
{
public:
    [[nodiscard]] static MmcssRegistration enable(const wchar_t* taskName);

    MmcssRegistration(const MmcssRegistration&) = delete;
    MmcssRegistration& operator=(const MmcssRegistration&) = delete;
    MmcssRegistration(MmcssRegistration&& other) noexcept;
    MmcssRegistration& operator=(MmcssRegistration&&) = delete;
    ~MmcssRegistration();

private:
    MmcssRegistration(HANDLE handle, DWORD taskIndex);

    HANDLE handle_ = nullptr;
    DWORD taskIndex_ = 0;
};

// Stops Windows from lowering this process's execution speed when the window
// is not in the foreground, as it does for background work.
void disableProcessPowerThrottling();

} // namespace lynx::core
