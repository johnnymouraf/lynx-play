#include "core/FramePacer.h"

#include <array>

namespace lynx::core
{

namespace
{

// SetWaitableTimerEx takes 100 ns units; negative values are relative.
using TimerTicks = std::chrono::duration<LONGLONG, std::ratio<1, 10'000'000>>;

HANDLE createTimer()
{
    const HANDLE precise = CreateWaitableTimerExW(
        nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

    if (precise != nullptr)
    {
        return precise;
    }

    return CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
}

} // namespace

std::expected<FramePacer, std::string> FramePacer::create(double framesPerSecond)
{
    if (framesPerSecond <= 0.0)
    {
        return std::unexpected(std::string("the frame rate must be positive"));
    }

    UniqueHandle timer(createTimer());

    if (!timer)
    {
        return std::unexpected(
            describeFailure("CreateWaitableTimerExW", HRESULT_FROM_WIN32(GetLastError())));
    }

    UniqueHandle stopEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr));

    if (!stopEvent)
    {
        return std::unexpected(describeFailure("CreateEventW", HRESULT_FROM_WIN32(GetLastError())));
    }

    const std::chrono::duration<double> seconds(1.0 / framesPerSecond);
    const auto period = std::chrono::duration_cast<Clock::duration>(seconds);

    return FramePacer(std::move(timer), std::move(stopEvent), period);
}

FramePacer::FramePacer(UniqueHandle timer, UniqueHandle stopEvent, Clock::duration period)
    : timer_(std::move(timer)),
      stopEvent_(std::move(stopEvent)),
      period_(period),
      nextTick_(Clock::now())
{
}

bool FramePacer::waitForNextTick(const std::stop_token& stop)
{
    nextTick_ += period_;
    const Clock::time_point now = Clock::now();

    if (nextTick_ + period_ < now)
    {
        nextTick_ = now;
    }

    if (nextTick_ <= now)
    {
        return !stop.stop_requested();
    }

    const LARGE_INTEGER dueTime{
        .QuadPart = -std::chrono::duration_cast<TimerTicks>(nextTick_ - now).count(),
    };
    SetWaitableTimerEx(timer_.get(), &dueTime, 0, nullptr, nullptr, nullptr, 0);

    const std::stop_callback wakeOnStop(stop, [this] { SetEvent(stopEvent_.get()); });
    const std::array handles{timer_.get(), stopEvent_.get()};
    WaitForMultipleObjects(static_cast<DWORD>(handles.size()), handles.data(), FALSE, INFINITE);

    return !stop.stop_requested();
}

} // namespace lynx::core
