/*
 *  Window.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/Math/float2.hpp"


namespace bee {


#ifndef BEE_MAX_MONITORS
    #define BEE_MAX_MONITORS 16
#endif // BEE_MAX_MONITORS

#ifndef BEE_MAX_WINDOWS
    #define BEE_MAX_WINDOWS 32
#endif // BEE_MAX_WINDOWS

BEE_RAW_HANDLE_I32(MonitorHandle);
BEE_RAW_HANDLE_I32(WindowHandle);

class Path;

struct Point
{
    i32 x { 0 };
    i32 y { 0 };

    inline float2 to_float2() const
    {
        return float2(static_cast<float>(x), static_cast<float>(y));
    }
};

struct MonitorInfo
{
    MonitorHandle   handle;
    i32             display_index { 0 };
    Point           size;
    Point           position;
};

struct WindowCreateInfo
{
    const char*     title { "Bee Application" };
    MonitorHandle   monitor;
    bool            fullscreen { false };
    bool            borderless { false };
    bool            allow_resize { true };
    bool            centered { true };
    Point           position;
    Point           size { 800, 600 };
};

#define BEE_PLATFORM_MODULE_NAME "BEE_PLATFORM_MODULE"

struct PlatformModule
{
    bool (*start)(const char* app_name) { nullptr };

    bool (*shutdown)() { nullptr };

    bool (*is_running)() { nullptr };

    bool (*quit_requested)() { nullptr };

    void (*poll_input)() { nullptr };

    i32 (*enumerate_monitors)(MonitorInfo* dst) { nullptr };

    const MonitorInfo* (*get_primary_monitor)() { nullptr };

    WindowHandle (*create_window)(const WindowCreateInfo& info) { nullptr };

    void (*destroy_window)(const WindowHandle handle) { nullptr };

    void (*destroy_all_windows)() { nullptr };

    Point (*get_window_size)(const WindowHandle handle) { nullptr };

    Point (*get_framebuffer_size)(const WindowHandle handle) { nullptr };

    bool (*window_close_requested)(const WindowHandle handle) { nullptr };

    void* (*get_os_window)(const WindowHandle handle) { nullptr };

    Point (*get_cursor_position)(const WindowHandle handle) { nullptr };

    bool (*is_minimized)(const WindowHandle handle) { nullptr };

    bool (*is_maximized)(const WindowHandle handle) { nullptr };

    bool (*has_focus)(const WindowHandle handle) { nullptr };

    /*
     ************
     *
     * Dialogs
     *
     ************
     */
    bool (*open_file_dialog)(Path* dst) { nullptr };
};


} // namespace bee
