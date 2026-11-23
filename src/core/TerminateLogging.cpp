#include "core/TerminateLogging.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <format>
#include <string>
#include <string_view>
#include <typeinfo>

#include <spdlog/spdlog.h>

#include <windows.h>

namespace lynx::core
{

namespace
{

constexpr WORD kMaxFrames = 64;

std::terminate_handler previousHandler = nullptr;

// The file name of the module that holds `address`, with the address's offset
// into it: enough to name a frame without a symbol server. Fixed buffers keep
// allocation failures out of the terminate path.
std::string describeFrame(void* address)
{
    HMODULE module = nullptr;

    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(address), &module) == FALSE)
    {
        return "unknown";
    }

    std::array<wchar_t, MAX_PATH> path{};
    const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    std::wstring_view text(path.data(), length);
    const std::size_t separator = text.find_last_of(L"\\/");

    if (separator != std::wstring_view::npos)
    {
        text.remove_prefix(separator + 1);
    }

    std::string name;
    name.reserve(text.size());

    for (const wchar_t character : text)
    {
        name.push_back(character < 0x80 ? static_cast<char>(character) : '?');
    }

    const auto offset = reinterpret_cast<std::uintptr_t>(address) -
                        reinterpret_cast<std::uintptr_t>(module);

    return std::format("{}+0x{:x}", name, offset);
}

void logStack()
{
    std::array<void*, kMaxFrames> frames{};
    const USHORT count = RtlCaptureStackBackTrace(0, frames.size(), frames.data(), nullptr);

    for (USHORT index = 0; index < count; ++index)
    {
        spdlog::error("  frame {}: {}", index, describeFrame(frames[index]));
    }
}

void logActiveException(const char* context)
{
    const DWORD thread = GetCurrentThreadId();
    const std::exception_ptr exception = std::current_exception();

    if (exception == nullptr)
    {
        spdlog::error("{}: terminate was called without an active exception (thread {})", context,
                      thread);
        logStack();
        return;
    }

    try
    {
        std::rethrow_exception(exception);
    }
    catch (const std::exception& error)
    {
        spdlog::error("{}: {}: {} (thread {})", context, typeid(error).name(), error.what(),
                      thread);
    }
    catch (...)
    {
        spdlog::error("{}: an exception that is not a std::exception (thread {})", context, thread);
    }

    logStack();
}

void onTerminate()
{
    try
    {
        logActiveException("Unhandled exception");

        if (spdlog::default_logger() != nullptr)
        {
            spdlog::default_logger()->flush();
        }
    }
    catch (...)
    {
        // The log is best effort; the process must still end as before.
    }

    if (previousHandler != nullptr)
    {
        previousHandler();
    }

    std::abort();
}

} // namespace

void installTerminateLogging()
{
    previousHandler = std::set_terminate(onTerminate);
}

void logUnhandledException(const char* context)
{
    logActiveException(context);

    if (spdlog::default_logger() != nullptr)
    {
        spdlog::default_logger()->flush();
    }
}

} // namespace lynx::core
