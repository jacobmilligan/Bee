/*
 *  Win32Platform.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */


// We need the following macros to enable things like window messages etc.
#define BEE_MINWINDOWS_ENABLE_WINDOWING
#define BEE_MINWINDOWS_ENABLE_USER
#define BEE_MINWINDOWS_ENABLE_GDI
#define BEE_MINWINDOWS_ENABLE_MSG
#define BEE_MINWINDOWS_ENABLE_WINOFFSETS
#include "Bee/Core/Win32/MinWindows.h"
#include "Bee/Core/Containers/HandleTable.hpp"
#include "Bee/Core/Concurrency.hpp"

#include "Bee/Application/Platform.hpp"

namespace bee {


#ifndef BEE_WNDCLASSNAME
    #define BEE_WNDCLASSNAME L"BeeWindow"
#endif // !BEE_WNDCLASSNAME


struct Win32Monitor
{
    WCHAR   device_name[32];
    u32     device_id { 0 };
    float   width { 0.0f };
    float   height { 0.0f };
    float   x { 0.0f };
    float   y { 0.0f };
};


struct Win32Window
{
    HWND            hwnd { nullptr };
    Thread::id_t    owning_thread { 0 };
    bool            is_close_requested { false };
};

using window_table_t = HandleTable<BEE_MAX_WINDOWS, WindowHandle, Win32Window>;


static struct PlatformData
{
    bool            is_launched { false };
    bool            is_quit_requested { false };
    i32             monitor_count { 0 };
    Win32Monitor    monitors[BEE_MAX_MONITORS];
    window_table_t  windows;
} g_platform;


LRESULT CALLBACK g_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


bool os_launch(const char* app_name)
{
    platform_discover_monitors();

    WNDCLASSEXW wndclass{};
    wndclass.cbSize = sizeof(wndclass);
    wndclass.style = CS_HREDRAW | CS_VREDRAW
        | CS_OWNDC; // vertical and horizontal redraw + own device per window
    wndclass.lpfnWndProc = g_window_proc;
    // wndclass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wndclass.lpszClassName = BEE_WNDCLASSNAME;

    auto err = RegisterClassExW(&wndclass);
    BEE_ASSERT_F(err != 0, "Failed to register a Win32 window class: %s", win32_get_last_error_string());

    return true;
}

void os_quit()
{
    g_platform.is_quit_requested = true;
    const auto result = UnregisterClassW(BEE_WNDCLASSNAME, GetModuleHandleW(nullptr));
    BEE_ASSERT_F(result != 0, "Failed to unregister a Win32 window class: %s", win32_get_last_error_string());
}


bool platform_is_launched()
{
    return g_platform.is_launched;
}


void platform_discover_monitors()
{
    g_platform.monitor_count = 0;
    memset(g_platform.monitors, 0, sizeof(Win32Monitor) * static_array_length(g_platform.monitors));

    DISPLAY_DEVICEW display{};
    DISPLAY_DEVICEW adapter{};
    DWORD adapter_index = 0;
    DWORD display_index = 0;
    DEVMODEW devmode{};

    while (g_platform.monitor_count < BEE_MAX_MONITORS)
    {
        adapter.cb = sizeof(adapter);
        // Get the next adapter
        if (EnumDisplayDevicesW(nullptr, adapter_index++, &adapter, 0) == 0)
        {
            break;
        }

        if (!(adapter.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
        {
            continue;
        }

        //  Query all displays associated with an adapter
        display_index = 0;
        while (g_platform.monitor_count < BEE_MAX_MONITORS)
        {
            display.cb = sizeof(display);
            if (EnumDisplayDevicesW(adapter.DeviceName, display_index, &display, 0) == 0)
            {
                break;
            }

            devmode.dmSize = sizeof(devmode);
            if (EnumDisplaySettingsW(adapter.DeviceName, ENUM_CURRENT_SETTINGS, &devmode) != 0)
            {
                auto& monitor = g_platform.monitors[g_platform.monitor_count];
                memcpy(monitor.device_name, display.DeviceName, 32 * sizeof(WCHAR));
                monitor.device_id = display_index;
                monitor.width = static_cast<float>(devmode.dmPelsWidth);
                monitor.height = static_cast<float>(devmode.dmPelsHeight);
                monitor.x = static_cast<float>(devmode.dmPosition.x);
                monitor.y = static_cast<float>(devmode.dmPosition.y);

                ++g_platform.monitor_count;
            }

            ++display_index;
        }
    }
}


/*
 **********************************************************
 *
 * # Window-related functions
 *
 **********************************************************
 */
WindowHandle platform_create_window(const WindowConfig& config)
{
    BEE_ASSERT_F(platform_is_launched(), "Tried to create a window_handle before initializing the current platform");

    if (BEE_FAIL_F(g_platform.windows.size() < BEE_MAX_WINDOWS, "Created window limit has been reached"))
    {
        return WindowHandle();
    }

    DWORD style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE;
    if (config.borderless)
    {
        style |= WS_POPUP;
    }
    else
    {
        style |= WS_SYSMENU | WS_CAPTION;
    }
    if (config.allow_resize)
    {
        style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    }

    DWORD exstyle = WS_EX_APPWINDOW;

    const auto wsize = strlen(config.title) + 1;
    wchar_t title_buffer[4096];
    memset(title_buffer, 0, sizeof(wchar_t) * static_array_length(title_buffer));

    // Convert utf8 to win32 unicode
    mbstowcs(title_buffer, config.title, wsize);

    Win32Window new_window{};
    new_window.owning_thread = current_thread::id();
    new_window.hwnd = CreateWindowExW(
        exstyle,
        BEE_WNDCLASSNAME,
        title_buffer,
        style,
        config.x, config.y,
        config.width, config.height,
        nullptr, nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );

    BEE_ASSERT_F(new_window.hwnd != nullptr, "Win32 Window creation failed with error code: %s", win32_get_last_error_string());

    const auto handle = g_platform.windows.create(new_window);

    SetWindowLong(new_window.hwnd, GWLP_USERDATA, handle.id);

    return handle;
}

void platform_destroy_window(const WindowHandle& handle)
{
    auto window = g_platform.windows[handle];
    BEE_ASSERT(window->hwnd != nullptr);
    DestroyWindow(window->hwnd);
    g_platform.windows.destroy(handle);
}

void* platform_get_os_window(const WindowHandle& handle)
{
    return g_platform.windows[handle]->hwnd;
}


/*
 **********************************************************
 *
 * # Input and message pump
 *
 * Main message pump for all win32 windows
 *
 **********************************************************
 */
void platform_poll_input()
{
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}


/*
 **********************************************************
 *
 * # Global window procedure
 *
 * Main message pump for all win32 windows
 *
 **********************************************************
 */
LRESULT CALLBACK g_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    const LONG window_id = GetWindowLong(hwnd, GWLP_USERDATA);

    if (window_id <= 0)
    {
        // This should only happen on init_subsystems or when there's some kind of error (i.e. no valid windows found)
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    auto window = g_platform.windows[WindowHandle(window_id)];
    switch (uMsg)
    {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            // window->input->key_down(static_cast<u32>(wParam));
        }
            break;

        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            // window->input->key_up(static_cast<u32>(wParam));
        }
            break;

        case WM_CLOSE:
        {
            window->is_close_requested = true;
            return true;
        }

        case WM_QUIT:
        {
            g_platform.is_quit_requested = true;
            return true;
        }

        default:
            break;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}


} // namespace bee