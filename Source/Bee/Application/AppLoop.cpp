/*
 *  AppLoop.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Application/AppLoop.hpp"
#include "Bee/Application/Platform.hpp"

namespace bee {


int __internal_app_launch(const char* app_name)
{
    if (!platform_launch(app_name))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void __internal_app_tick()
{
    platform_poll_input();
}

void __internal_app_shutdown()
{
    platform_quit();
}


} // namespace bee