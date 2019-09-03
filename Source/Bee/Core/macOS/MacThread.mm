/*
 *  MacThread.mm
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Skyrocket/Platform/Thread.hpp"
#include "Skyrocket/Platform/Time.hpp"

#include <mach/thread_policy.h>
#include <mach/thread_act.h>

namespace bee {
namespace current_thread {

void Thread::set_affinity(const i32 cpu)
{
    const auto tag = cpu > affinity_none ? cpu : THREAD_AFFINITY_TAG_NULL;

    thread_affinity_policy_data_t policy_data = { tag };
    // FIXME(Jacob): this might be very wrong and do crazy stuff when the main or a null thread is used
    mach_port_t mach_thread = pthread_mach_thread_np(thread_data_.native_thread);
    thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy_data, THREAD_AFFINITY_POLICY_COUNT);
}

void thread_sleep(const u64 ticks_to_sleep)
{
    const auto seconds_in_ticks = ticks_to_sleep / TimePoint::ticks_per_second;

    timespec timespec_data{};
    timespec_data.tv_sec = (__darwin_time_t)math::min(seconds_in_ticks, static_cast<u64>(LONG_MAX));
    timespec_data.tv_nsec = 999999999; // (10^9 - 1)

    if (timespec_data.tv_sec < LONG_MAX) {
        const auto ticks_from_seconds = seconds_in_ticks * TimePoint::ticks_per_second;
        timespec_data.tv_nsec = static_cast<long>(ticks_to_sleep - ticks_from_seconds);
    }

    while (nanosleep(&timespec_data, &timespec_data) == -1 && errno == EINTR);
}


} // namespace current_thread
} // namespace bee
