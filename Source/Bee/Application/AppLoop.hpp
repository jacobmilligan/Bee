/*
 *  MainLoop.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/Error.hpp"
#include "Bee/Application/Platform.hpp"


namespace bee {


struct AppLaunchConfig
{
    const char*     app_name { nullptr };
    WindowConfig    main_window_config;
};


struct AppContext
{
    bool            quit { false };
    WindowHandle    main_window;
    InputBuffer     default_input;
};


struct BEE_RUNTIME_API Application
{
    virtual int launch(AppContext* ctx) = 0;

    virtual void shutdown(AppContext* ctx) = 0;

    virtual void tick(AppContext* ctx) = 0;
};


BEE_RUNTIME_API int app_loop(const AppLaunchConfig& config, Application* app);



} // namespace bee