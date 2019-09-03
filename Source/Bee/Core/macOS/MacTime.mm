/*
 *  MacTime.mm
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Skyrocket/Platform/Time.hpp"

#include <mach/mach_time.h>

namespace bee {
namespace time {

static mach_timebase_info_data_t get_timebase() noexcept
{
    mach_timebase_info_data_t timebase_info{};
    mach_timebase_info(&timebase_info);
    return timebase_info;
}

static u64 get_system_hertz() noexcept
{
    const auto timebase_info = get_timebase();
    return static_cast<u64>(timebase_info.denom * 1e9 / timebase_info.numer);
}

u64 now() noexcept
{
    static const mach_timebase_info_data_t timebase_info = get_timebase();
    // PowerPC architectures converted between ticks and nanos using different timebase scaling factors -
    // however, intel Macs already use nanos for system ticks, therefore we don't need to worry about
    // integer overflow with the multiplication here as no timebase scaling has occurred on Macs we support
    return mach_absolute_time() * timebase_info.numer / timebase_info.denom;
}

u64 ticks_per_second() noexcept
{
    static const auto hertz = get_system_hertz();
    return hertz;
}


} // namespace time
} // namespace bee
