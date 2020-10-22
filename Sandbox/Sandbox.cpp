/*
 *  Sandbox.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Main.hpp"
#include "Bee/Platform/Platform.hpp"
#include "Bee/Input/Input.hpp"
#include "Bee/Input/Keyboard.hpp"
#include "Bee/Input/Mouse.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Filesystem.hpp"


static bee::PlatformModule* g_platform = nullptr;
static bee::InputModule*    g_input = nullptr;

int bee_main(int argc, char** argv)
{
    bee::init_plugins();
    bee::add_plugin_search_path(bee::fs::get_root_dirs().binaries_root.join("Plugins"));
    bee::refresh_plugins();
    bee::load_plugin("Bee.Platform");

    g_platform = static_cast<bee::PlatformModule*>(bee::get_module(BEE_PLATFORM_MODULE_NAME));
    g_input = static_cast<bee::InputModule*>(bee::get_module(BEE_INPUT_MODULE_NAME));
    g_platform->start("Bee.Sandbox");

    bee::WindowCreateInfo window_info{};
    window_info.title = "Bee Sandbox";
    window_info.monitor = g_platform->get_primary_monitor()->handle;
    auto window = g_platform->create_window(window_info);

    const bee::InputDevice* keyboard = g_input->default_device(bee::InputDeviceType::keyboard);
    const bee::InputDevice* mouse = g_input->default_device(bee::InputDeviceType::mouse);

    if (keyboard == nullptr || mouse == nullptr)
    {
        g_platform->shutdown();
        return EXIT_FAILURE;
    }

    bee::log_info("Keyboard: %s", keyboard->name);
    bee::log_info("Mouse: %s", mouse->name);

    while (!g_platform->quit_requested() && !g_platform->window_close_requested(window))
    {
        bee::refresh_plugins();
        g_platform->poll_input();

        const bool escape_typed = keyboard->get_state(bee::Key::escape)->values[0].flag && !keyboard->get_previous_state(bee::Key::escape)->values[0].flag;
        if (escape_typed)
        {
            break;
        }

        if (mouse->get_state(bee::MouseButton::left)->values[0].flag && !mouse->get_previous_state(bee::MouseButton::left)->values[0].flag)
        {
            bee::log_info("Clicked!");
        }
    }

    if (window.is_valid())
    {
        g_platform->destroy_window(window);
    }

    g_platform->shutdown();
    return EXIT_SUCCESS;
}