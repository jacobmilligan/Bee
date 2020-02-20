/*
 *  MainLoop.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/Error.hpp"
#include "Bee/Application/Platform.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"


namespace bee {


struct AppContext // NOLINT
{
    bool            quit { false };
    WindowHandle    main_window;
    InputBuffer     default_input;
    void*           user_data { nullptr };
};

struct JobSystemInitInfo;

struct AppDescriptor
{
    const char*         app_name { nullptr };
    WindowConfig        main_window_config;
    JobSystemInitInfo   job_system_info;
    void*               user_data { nullptr };

    int (*on_launch)(AppContext* ctx) { nullptr };
    void (*on_shutdown)(AppContext* ctx) { nullptr };
    void (*on_frame)(AppContext* ctx) { nullptr };
};

BEE_RUNTIME_API int app_run(const AppDescriptor& desc);


} // namespace bee