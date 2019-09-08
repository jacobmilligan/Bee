/*
 *  Platform.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Application/Input.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/Serialization/Serialization.hpp"


#ifndef BEE_MAX_MONITORS
    #define BEE_MAX_MONITORS 8
#endif // BEE_MAX_MONITORS

#ifndef BEE_MAX_WINDOWS
    #define BEE_MAX_WINDOWS 16
#endif // BEE_MAX_WINDOWS

namespace bee {


BEE_DEFINE_RAW_HANDLE_I32(Monitor);
BEE_DEFINE_VERSIONED_HANDLE(Window);


struct PlatformSize
{
    i32 width { 0 };
    i32 height { 0 };
};


struct WindowConfig
{
    const char*     title { "Skyrocket Application" }; // nonserialized
    MonitorHandle   monitor; // nonserialized
    bool            fullscreen { false };
    bool            borderless { false };
    bool            allow_resize { true };
    bool            centered { true };
    i32             width { 800 };
    i32             height { 600 };
    i32             x { 0 };
    i32             y { 0 };
};


BEE_SERIALIZE(WindowConfig, 1)
{
    BEE_ADD_FIELD(1, fullscreen);
    BEE_ADD_FIELD(1, borderless);
    BEE_ADD_FIELD(1, allow_resize);
    BEE_ADD_FIELD(1, centered);
    BEE_ADD_FIELD(1, width);
    BEE_ADD_FIELD(1, height);
    BEE_ADD_FIELD(1, x);
    BEE_ADD_FIELD(1, y);
}


BEE_API bool platform_launch(const char* app_name);

BEE_API void platform_shutdown();

BEE_API bool platform_is_running();

BEE_API bool platform_quit_requested();

BEE_API void discover_monitors();

BEE_API WindowHandle create_window(const WindowConfig& config);

BEE_API void destroy_window(const WindowHandle& handle);

BEE_API void* get_os_window(const WindowHandle& handle);

BEE_API void destroy_all_open_windows();

BEE_API PlatformSize get_window_size(const WindowHandle& handle);

BEE_API PlatformSize get_window_framebuffer_size(const WindowHandle& handle);

BEE_API void poll_input(InputBuffer* input_buffer);


} // namespace bee