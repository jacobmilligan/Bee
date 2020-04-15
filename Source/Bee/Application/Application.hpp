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


struct AppContext
{
    bool            quit { false };
    WindowHandle    main_window;
    InputBuffer     default_input;
    i32             argc { 0 };
    const char*     argv { nullptr };
    void*           user_data { nullptr };
};

struct AppDescriptor
{
    const char*         app_name { nullptr };
    WindowConfig        main_window_config;
};

#define BEE_APPLICATION_API_NAME "BEE_APPLICATION_API"

struct ApplicationApi
{
    void    (*configure)(AppDescriptor* desc) { nullptr };
    int     (*launch)(AppContext* ctx) { nullptr };
    void    (*shutdown)(AppContext* ctx) { nullptr };
    void    (*fail)(AppContext* ctx) { nullptr };
    void    (*tick)(AppContext* ctx) { nullptr };
};


} // namespace bee