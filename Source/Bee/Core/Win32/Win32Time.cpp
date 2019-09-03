//
//  Win32Time.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 02/01/2019
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#include "Bee/Core/Win32/MinWindows.h"
#include "Bee/Core/Time.hpp"
#include "Bee/Core/Error.hpp"

//// for timeGetTime()
#include <timeapi.h>

namespace bee {
namespace time {


u64 now() noexcept
{
    LARGE_INTEGER now;
    const auto result = QueryPerformanceCounter(&now);

    if (!BEE_CHECK(result != 0)) {
        now.QuadPart = timeGetTime();
    }

    // ticks
    return static_cast<u64>(now.QuadPart);
}

static LARGE_INTEGER get_frequency() noexcept
{
    LARGE_INTEGER frequency;
    const auto result = QueryPerformanceFrequency(&frequency);
    BEE_ASSERT_F(result != 0, "Unable to query the systems high-resolution performance counter");
    return frequency;
}

u64 ticks_per_second() noexcept
{
    static LARGE_INTEGER frequency = get_frequency();
    return static_cast<u64>(frequency.QuadPart);
}


} // namespace time
} // namespace bee
