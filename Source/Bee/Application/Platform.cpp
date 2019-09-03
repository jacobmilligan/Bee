/*
 *  Platform.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Application/Platform.hpp"

namespace bee {


/*
 **********************************************************
 *
 * # OS-specific FWD decl
 *
 * Calls into the OS-specific functions for launching,
 * quitting, polling input etc.
 *
 **********************************************************
 */
bool os_launch(const char* app_name);

void os_quit();


/*
 **********************************************************
 *
 * # OS-agnostic calls
 *
 **********************************************************
 */
bool platform_launch(const char* app_name)
{
    BEE_ASSERT_F(!platform_is_launched(), "Platform is already launched and running");
    if (!os_launch(app_name))
    {
        return false;
    }

    return true;
}

void platform_quit()
{
    os_quit();
}


} // namespace bee