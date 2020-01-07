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


struct BEE_REFLECT(serializable) WindowConfig
{
    BEE_REFLECT(nonserialized)
    const char*     title { "Bee Application" };
    BEE_REFLECT(nonserialized)
    MonitorHandle   monitor;
    bool            fullscreen { false };
    bool            borderless { false };
    bool            allow_resize { true };
    bool            centered { true };
    i32             width { 800 };
    i32             height { 600 };
    i32             x { 0 };
    i32             y { 0 };
};


BEE_RUNTIME_API bool platform_launch(const char* app_name);

BEE_RUNTIME_API void platform_shutdown();

BEE_RUNTIME_API bool platform_is_running();

BEE_RUNTIME_API bool platform_quit_requested();

BEE_RUNTIME_API void discover_monitors();

BEE_RUNTIME_API WindowHandle create_window(const WindowConfig& config);

BEE_RUNTIME_API void destroy_window(const WindowHandle& handle);

BEE_RUNTIME_API void* get_os_window(const WindowHandle& handle);

BEE_RUNTIME_API void destroy_all_open_windows();

BEE_RUNTIME_API PlatformSize get_window_size(const WindowHandle& handle);

BEE_RUNTIME_API PlatformSize get_window_framebuffer_size(const WindowHandle& handle);

BEE_RUNTIME_API void poll_input(InputBuffer* input_buffer);


} // namespace bee