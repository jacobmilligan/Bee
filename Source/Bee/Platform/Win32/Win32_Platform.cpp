/*
 *  Win32Platform.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/Platform/Platform.hpp"

// We need the following macros to enable things like window messages etc.
#define BEE_MINWINDOWS_ENABLE_WINDOWING
#define BEE_MINWINDOWS_ENABLE_USER
#define BEE_MINWINDOWS_ENABLE_GDI
#define BEE_MINWINDOWS_ENABLE_MSG
#define BEE_MINWINDOWS_ENABLE_WINOFFSETS
#include "Bee/Platform/Win32/Win32_RawInput.hpp"


namespace bee {


/*
 ********************************
 *
 * Win32 Platform implementation
 *
 ********************************
 */
#ifndef BEE_WNDCLASSNAME
    #define BEE_WNDCLASSNAME L"BeeWindow"
#endif // !BEE_WNDCLASSNAME

struct Monitor // NOLINT
{
    bool        is_primary_device { false };
    WCHAR       device_name[32];
    MonitorInfo info;
};

struct Window // NOLINT
{
    bool            close_requested { false };
    HWND            hwnd { nullptr };
    RAWINPUTDEVICE  device_ids[2];
};

struct Platform // NOLINT
{
    bool        is_running { false };
    bool        quit_requested { false };
    i32         monitor_count { 0 };
    i32         primary_monitor { -1 };
    i32         window_count { 0 };
    Monitor     monitors[BEE_MAX_MONITORS];
    Window      windows[BEE_MAX_WINDOWS];

    RawInput    raw_input;
};

static Platform* g_platform = nullptr;
extern RawInput* g_raw_input;


// TODO(Jacob) call this when we get the WM_DISPLAYCHANGE message in window proc
static void discover_monitors()
{
    DISPLAY_DEVICEW display{};
    DISPLAY_DEVICEW adapter{};
    DWORD adapter_index = 0;
    DEVMODEW devmode{};

    g_platform->monitor_count = 0;

    while (g_platform->monitor_count < static_array_length(g_platform->monitors))
    {
        adapter.cb = sizeof(adapter);

        // Get the next adapter
        if (EnumDisplayDevicesW(nullptr, adapter_index++, &adapter, 0) == 0)
        {
            break;
        }

        if ((adapter.StateFlags & DISPLAY_DEVICE_ACTIVE) == 0)
        {
            // monitor connected but turned off
            continue;
        }

        //  Query all displays associated with an adapter
        for (int display_index = 0; display_index < static_array_length(g_platform->monitors); ++display_index)
        {
            display.cb = sizeof(display);
            if (EnumDisplayDevicesW(adapter.DeviceName, static_cast<DWORD>(display_index), &display, 0) == 0)
            {
                break;
            }

            devmode.dmSize = sizeof(devmode);
            if (EnumDisplaySettingsW(adapter.DeviceName, ENUM_CURRENT_SETTINGS, &devmode) != 0)
            {
                auto& monitor = g_platform->monitors[g_platform->monitor_count];

                memcpy(monitor.device_name, display.DeviceName, 32 * sizeof(WCHAR));
                monitor.is_primary_device = (adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;
                monitor.info.handle.id = g_platform->monitor_count;
                monitor.info.display_index = display_index;
                monitor.info.size = { static_cast<i32>(devmode.dmPelsWidth), static_cast<i32>(devmode.dmPelsHeight) };
                monitor.info.position = { static_cast<i32>(devmode.dmPosition.x), static_cast<i32>(devmode.dmPosition.y) };

                if (monitor.info.position.x == 0 && monitor.info.position.y == 0)
                {
                    g_platform->primary_monitor = g_platform->monitor_count;
                }

                ++g_platform->monitor_count;
            }
        }
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
static LRESULT CALLBACK g_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    const LONG window_id = GetWindowLong(hwnd, GWLP_USERDATA);

    if (window_id < 0 || window_id >= BEE_MAX_WINDOWS)
    {
        // This should only happen on init_subsystems or when there's some kind of error (i.e. no valid windows found)
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    auto& window = g_platform->windows[window_id];
    switch (uMsg)
    {
        case WM_INPUT:
        {
            process_raw_input(lParam);
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            return 0;
        }

        case WM_CLOSE:
        {
            window.close_requested = true;
            return 0;
        }

        case WM_QUIT:
        {
            g_platform->quit_requested = true;
            return 0;
        }

        case WM_DISPLAYCHANGE:
        {
            discover_monitors();
            return 0;
        }

        default: break;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

bool start(const char* app_name)
{
    if (BEE_FAIL_F(!g_platform->is_running, "Platform is already running"))
    {
        return false;
    }

    discover_monitors();

    WNDCLASSEXW wndclass{};
    wndclass.cbSize = sizeof(wndclass);
    wndclass.style = CS_HREDRAW | CS_VREDRAW
        | CS_OWNDC; // vertical and horizontal redraw + own device per window
    wndclass.lpfnWndProc = g_window_proc;
    // wndclass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wndclass.lpszClassName = BEE_WNDCLASSNAME;

    auto err = RegisterClassExW(&wndclass);
    BEE_ASSERT_F(err != 0, "Failed to register a Win32 window class: %s", win32_get_last_error_string());

    register_input_devices();

    g_platform->is_running = true;
    return true;
}

bool shutdown()
{
    if (BEE_FAIL_F(g_platform->is_running, "Platform is already shut down"))
    {
        return false;
    }

    g_platform->quit_requested = true;
    unregister_input_devices();

    const auto result = UnregisterClassW(BEE_WNDCLASSNAME, GetModuleHandleW(nullptr));
    BEE_ASSERT_F(result != 0, "Failed to unregister a Win32 window class: %s", win32_get_last_error_string());

    g_platform->is_running = false;
    return true;
}

bool is_running()
{
    return g_platform->is_running;
}

bool quit_requested()
{
    return g_platform->quit_requested;
}

i32 enumerate_monitors(MonitorInfo* dst)
{
    if (dst == nullptr)
    {
        return g_platform->monitor_count;
    }

    for (int i = 0; i < g_platform->monitor_count; ++i)
    {
        memcpy(&dst[i], &g_platform->monitors[i].info, sizeof(MonitorInfo));
    }

    return g_platform->monitor_count;
}

const MonitorInfo* get_primary_monitor()
{
    if (g_platform->monitor_count == 0 || g_platform->primary_monitor < 0)
    {
        return nullptr;
    }

    return &g_platform->monitors[g_platform->primary_monitor].info;
}

WindowHandle create_window(const WindowCreateInfo& info)
{
    if (BEE_FAIL_F(g_platform->window_count < BEE_MAX_WINDOWS, "Created window limit has been reached"))
    {
        return WindowHandle{};
    }

    DWORD style = WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE;
    if (info.borderless)
    {
        style |= WS_POPUP;
    }
    else
    {
        style |= WS_SYSMENU | WS_CAPTION;
    }
    if (info.allow_resize)
    {
        style |= WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX;
    }

    DWORD exstyle = WS_EX_APPWINDOW;

    wchar_t title_buffer[4096];
    if (str::to_wchar(info.title, title_buffer, static_array_length(title_buffer)) == 0)
    {
        return WindowHandle{};
    }

    // find a free window
    const i32 index = find_index_if(g_platform->windows, [&](const Window& w)
    {
        return w.hwnd == nullptr;
    });

    BEE_ASSERT(index >= 0);

    Point monitor_pos;

    if (info.monitor.is_valid())
    {
        if (info.monitor.id < g_platform->monitor_count)
        {
            monitor_pos = g_platform->monitors[info.monitor.id].info.position;
        }
        else
        {
            log_warning("Invalid monitor id passed to create_window: %d", info.monitor.id);
        }
    }

    Window& window = g_platform->windows[index];
    window.hwnd = CreateWindowExW(
        exstyle,
        BEE_WNDCLASSNAME,
        title_buffer,
        style,
        monitor_pos.x + info.position.x, monitor_pos.y + info.position.y,
        info.size.x, info.size.y,
        nullptr, nullptr,
        GetModuleHandleW(nullptr),
        nullptr
    );

    BEE_ASSERT_F(window.hwnd != nullptr, "Win32 Window creation failed with error code: %s", win32_get_last_error_string());

    SetWindowLong(window.hwnd, GWLP_USERDATA, index);

    if (BEE_FAIL_F(register_raw_input(window.hwnd, RIDEV_INPUTSINK), "Failed to create win32 window: %s", win32_get_last_error_string()))
    {
        DestroyWindow(window.hwnd);
        window.hwnd = nullptr;
        return WindowHandle{};
    }

    ++g_platform->window_count;
    return WindowHandle { index };
}

void destroy_window(const WindowHandle handle)
{
    BEE_ASSERT(handle.is_valid());

    auto& window = g_platform->windows[handle.id];
    BEE_ASSERT_F(window.hwnd != nullptr, "Window was already destroyed");

    if (!register_raw_input(window.hwnd, RIDEV_REMOVE))
    {
        log_error("Failed to unregister raw input from win32 window: %s", win32_get_last_error_string());
    }

    DestroyWindow(window.hwnd);
    window.hwnd = nullptr;
    --g_platform->window_count;
}

void destroy_all_windows()
{
    for (auto& window : g_platform->windows)
    {
        if (window.hwnd != nullptr)
        {
            DestroyWindow(window.hwnd);
            window.hwnd = nullptr;
        }
    }

    g_platform->window_count = 0;
}

Point get_window_size(const WindowHandle handle)
{
    auto& window = g_platform->windows[handle.id];
    BEE_ASSERT(window.hwnd != nullptr);

    RECT rect{};
    const auto success = GetClientRect(window.hwnd, &rect);
    BEE_ASSERT_F(success == TRUE, "Failed to get window size: %s", win32_get_last_error_string());

    Point size{};
    size.x = rect.right;
    size.y = rect.bottom;
    return size;
}

Point get_framebuffer_size(const WindowHandle handle)
{
    return get_window_size(handle);
}

bool is_window_close_requested(const WindowHandle handle)
{
    auto& window = g_platform->windows[handle.id];
    return window.hwnd == nullptr || window.close_requested;
}

void* get_os_window(const WindowHandle handle)
{
    return g_platform->windows[handle.id].hwnd;
}

Point get_cursor_position(const WindowHandle handle)
{
    auto* hwnd = g_platform->windows[handle.id].hwnd;
    POINT pt;
    if (::GetCursorPos(&pt) == FALSE)
    {
        return Point{};
    }

    if (::ScreenToClient(hwnd, &pt) == FALSE)
    {
        return Point{};
    }

    return Point { pt.x, pt.y };
}

void poll_input()
{
    reset_raw_input();

    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);

        switch (msg.message)
        {
            case WM_INPUT:
            {
                process_raw_input(msg.lParam);
                break;
            }
            default: break;
        }
    }
}


} // namespace bee


bee::PlatformModule g_module;

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    bee::g_platform = loader->get_static<bee::Platform>("BeePlatformData");
    bee::g_raw_input = &bee::g_platform->raw_input;

    g_module.start = bee::start;
    g_module.shutdown = bee::shutdown;
    g_module.is_running = bee::is_running;
    g_module.quit_requested = bee::quit_requested;
    g_module.enumerate_monitors = bee::enumerate_monitors;
    g_module.get_primary_monitor = bee::get_primary_monitor;
    g_module.create_window = bee::create_window;
    g_module.destroy_window = bee::destroy_window;
    g_module.destroy_all_windows = bee::destroy_all_windows;
    g_module.get_window_size = bee::get_window_size;
    g_module.get_framebuffer_size = bee::get_framebuffer_size;
    g_module.window_close_requested = bee::is_window_close_requested;
    g_module.get_os_window = bee::get_os_window;
    g_module.get_cursor_position = bee::get_cursor_position;
    g_module.poll_input = bee::poll_input;

    loader->require_plugin("Bee.Input", { 0, 0, 0 });
    loader->set_module(BEE_PLATFORM_MODULE_NAME, &g_module, state);
}
