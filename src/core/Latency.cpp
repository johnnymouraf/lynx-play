#include "core/Latency.h"

#include "core/Windows.h"

#include <utility>

#include <avrt.h>
#include <spdlog/spdlog.h>

namespace lynx::core
{

MmcssRegistration::MmcssRegistration(HANDLE handle, DWORD taskIndex)
    : handle_(handle), taskIndex_(taskIndex)
{
}

MmcssRegistration MmcssRegistration::enable(const wchar_t* taskName)
{
    DWORD taskIndex = 0;
    HANDLE handle = AvSetMmThreadCharacteristicsW(taskName, &taskIndex);

    if (handle == nullptr)
    {
        spdlog::warn("{}", describeFailure("AvSetMmThreadCharacteristicsW",
                                           HRESULT_FROM_WIN32(GetLastError())));
        return MmcssRegistration(nullptr, 0);
    }

    spdlog::info("MMCSS: '{}' task registered on thread {}",
                 toUtf8(taskName), GetCurrentThreadId());

    return MmcssRegistration(handle, taskIndex);
}

MmcssRegistration::MmcssRegistration(MmcssRegistration&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)),
      taskIndex_(std::exchange(other.taskIndex_, 0))
{
}

MmcssRegistration::~MmcssRegistration()
{
    if (handle_ != nullptr)
    {
        AvRevertMmThreadCharacteristics(handle_);
    }
}

void disableProcessPowerThrottling()
{
    PROCESS_POWER_THROTTLING_STATE state{};
    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    state.StateMask = 0;

    if (const HRESULT result =
            SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &state,
                                  sizeof(state));
        FAILED(result))
    {
        spdlog::warn("{}", describeFailure("SetProcessInformation (power throttling)", result));
        return;
    }

    spdlog::info("Process power throttling: execution speed stays high in the background");
}

} // namespace lynx::core
