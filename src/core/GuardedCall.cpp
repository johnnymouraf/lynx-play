#include "core/GuardedCall.h"

#include "core/Windows.h"

namespace mfplay::core
{

// The function holds no objects that need unwinding, which SEH requires.
__declspec(noinline) bool callGuarded(const std::function<void()>& call, std::uint32_t& fault)
{
    bool completed = false;

    __try
    {
        call();
        completed = true;
    }
    __except (fault = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
    {
        completed = false;
    }

    return completed;
}

} // namespace mfplay::core
