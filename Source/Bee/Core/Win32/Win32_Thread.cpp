/*
 *  Win32Thread.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Win32/MinWindows.h"
#include "Bee/Core/Thread.hpp"
#include "Bee/Core/Time.hpp"
#include "Bee/Core/String.hpp"
#include "Bee/Core/Enum.hpp"

namespace bee {


void set_native_thread_name(HANDLE native_thread, const StringView& name)
{
    WCHAR wc_name[BEE_THREAD_MAX_NAME]{ 0 };
    const auto convert_success = mbstowcs(wc_name, name.c_str(), static_cast<size_t>(name.size()));

    if (BEE_CHECK_F(convert_success != static_cast<size_t>(-1), "Thread: unable to convert thread name '%" BEE_PRIsv "' to wide string", BEE_FMT_SV(name)))
    {
        const auto result = SetThreadDescription(native_thread, wc_name);
        BEE_ASSERT_F(result >= 0, "Thread: couldn't set thread name to '%" BEE_PRIsv "': %s", BEE_FMT_SV(name), win32_get_last_error_string());
    }
}

BEE_TRANSLATION_TABLE(translate_thread_priority, ThreadPriority, int, ThreadPriority::unknown,
    THREAD_PRIORITY_IDLE, // idle,
    THREAD_PRIORITY_LOWEST, // lowest,
    THREAD_PRIORITY_BELOW_NORMAL, // below_normal,
    THREAD_PRIORITY_NORMAL, // normal,
    THREAD_PRIORITY_ABOVE_NORMAL, // above_normal,
    THREAD_PRIORITY_HIGHEST, // highest,
    THREAD_PRIORITY_TIME_CRITICAL, // time_critical,
)

namespace current_thread {


thread_id_t id()
{
    const thread_id_t thread_id = GetCurrentThreadId();
    return thread_id;
}

void sleep(const u64 ticks_to_sleep)
{
    const auto milliseconds = ticks_to_sleep / time::ticks_per_millisecond();
    const auto start_time = time::now();

    // According to MSDN, a millisecond value of 0 will cause the current thread to relinquish the rest of
    // it's time-slice to any thread of EQUAL PRIORITY, so rather than checking if milliseconds is less than 1
    // we can just let the thread possibly give up it's time-slice and potentially get less-than-millisecond
    // precision
    // see: https://docs.microsoft.com/en-us/windows/desktop/api/synchapi/nf-synchapi-sleep
    Sleep(static_cast<DWORD>(milliseconds));

    const auto now = time::now();
    const auto time_spent_sleeping = now - start_time;

    // for nanosecond precision (to mimic nanosleep) just spin wait for the remaining time
    if (time_spent_sleeping < ticks_to_sleep)
    {
        // get an absolute time from now representing the target sleep time and repeatedly test to see if it's elapsed
        const auto expected_done_time = now + ticks_to_sleep - time_spent_sleeping;
        while (true)
        {
            if (time::now() >= expected_done_time)
            {
                break;
            }
        }
    }
}

void set_affinity(const i32 cpu)
{
    auto new_affinity_mask = static_cast<DWORD_PTR>(1) << cpu;
    const auto affinity_success = SetThreadAffinityMask(GetCurrentThread(), new_affinity_mask);
    BEE_ASSERT_F(affinity_success != 0, "Thread: failed to set CPU affinity");
}

void set_name(const char* name)
{
    set_native_thread_name(GetCurrentThread(), name);
}

void set_priority(ThreadPriority priority)
{
    const auto success = SetThreadPriority(GetCurrentThread(), translate_thread_priority(priority));
    BEE_ASSERT_F(success != 0, "Failed to set thread priority: %s", win32_get_last_error_string());
}


} // current_thread


void Thread::join()
{
    const auto join_success = WaitForSingleObject(native_thread_, INFINITE);
    BEE_ASSERT_F(join_success != WAIT_FAILED, "Thread: failed to join thread: Win32 error code: %lu", GetLastError());
    native_thread_ = nullptr;
    name_.clear();
}

void Thread::detach()
{
    const auto close_handle_result = CloseHandle(native_thread_);
    BEE_ASSERT_F(close_handle_result != 0, "Thread: failed to detach thread: Win32 error code: %lu", GetLastError());
}

void Thread::set_affinity(const i32 cpu)
{
    BEE_ASSERT_F(native_thread_ != nullptr, "Thread: cannot set affinity for invalid thread");
    auto new_affinity_mask = static_cast<DWORD_PTR>(1) << cpu;
    const auto affinity_success = SetThreadAffinityMask(native_thread_, new_affinity_mask);
    BEE_ASSERT_F(affinity_success != 0, "Thread: failed to set CPU affinity");
}

void Thread::set_priority(const ThreadPriority priority)
{
    const auto success = SetThreadPriority(native_thread_, translate_thread_priority(priority));
    BEE_ASSERT_F(success != 0, "Failed to set thread priority: %s", win32_get_last_error_string());
}

thread_id_t Thread::id() const
{
    BEE_ASSERT(native_thread_ != nullptr);
    const auto id = GetThreadId(native_thread_);
    if (BEE_CHECK_F(id != 0, "Thread: failed to get thread id: %s", win32_get_last_error_string()))
    {
        return id;
    }
    return 0;
}

void Thread::create_native_thread(ExecuteParams* params) noexcept
{
    native_thread_ = CreateThread(
        nullptr,                // don't allow processes to inherit threads
        0,                      // default stack size
        execute_cb,             // start routine
        params,                 // parameter to pass
        0,                      // create suspended so we can set the thread data before running
        nullptr                 // don't get thread id now, the user will get this via Thread::id()
    );

    BEE_ASSERT_F(native_thread_ != nullptr, "Thread: unable to create native thread: Win32 error code: %lu", GetLastError());

    set_native_thread_name(native_thread_, name_.view());
}

Thread::execute_cb_return_t Thread::execute_cb(void* params)
{
    static constexpr auto access_violation_code = (DWORD)0xC0000005L;

    if (BEE_FAIL_F(params != nullptr, "Thread: invalid config given to callback"))
    {
        return access_violation_code;
    }

    auto* data = static_cast<Thread::ExecuteParams*>(params);
    if (BEE_FAIL_F(data->invoker != nullptr, "Invalid thread function given"))
    {
        return access_violation_code;
    }

    // register with temp allocator if needed
    const auto register_with_temp_allocator = data->register_with_temp_allocator;
    if (register_with_temp_allocator)
    {
        temp_allocator_register_thread();
    }

    // run the threads function
    data->invoker(data->function, data->arg);
    data->destructor(data->function, data->arg);

    if (register_with_temp_allocator)
    {
        temp_allocator_unregister_thread();
    }
    return 0;
}


}