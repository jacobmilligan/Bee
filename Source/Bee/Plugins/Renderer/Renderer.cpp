/*
 *  Renderer.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Plugins/Renderer/Renderer.hpp"
#include "Bee/Core/Plugin.hpp"

namespace bee {


struct RenderModuleData
{
    String name;
};


static RendererApi                      g_api;
static DeviceHandle                     g_device;
static DynamicArray<RenderModuleApi*>   g_modules;
static DynamicArray<RenderModuleData>   g_modules_data;


void render_module_plugin_observer(const PluginEventType event, const char* plugin_name, void* interface, void* user_data)
{
    auto* module = static_cast<RenderModuleApi*>(interface);

    if (event == PluginEventType::add_interface)
    {
        const auto module_index = container_index_of(g_modules, [&](const RenderModuleApi* m)
        {
            return m == module;
        });

        if (module_index >= 0)
        {
            log_error("RenderModule %s was already added to the renderer", module->get_name != nullptr ? module->get_name() : "<unknown>");
            return;
        }

        g_modules.push_back(module);
        g_modules_data.emplace_back();

        auto& module_data = g_modules_data.back();

        if (module->get_name != nullptr)
        {
            module_data.name = module->get_name();
        }
        else
        {
            module_data.name = str::format("RenderModule(%d)", g_modules.size() - 1);
        }

        module->create_resources(g_device);
    }

    if (event == PluginEventType::remove_interface)
    {
        const auto module_index = container_index_of(g_modules, [&](const RenderModuleApi* m)
        {
            return m == module;
        });

        if (module_index < 0)
        {
            log_error("RenderModule %s was not previously added to the renderer", module->get_name != nullptr ? module->get_name() : "<unknown>");
            return;
        }

        g_modules.erase(module_index);
        g_modules_data.erase(module_index);

        module->destroy_resources(g_device);
    }
}

bool init_renderer(const DeviceCreateInfo& device_info)
{
    if (BEE_FAIL_F(!g_device.is_valid(), "Renderer is already initialized"))
    {
        return false;
    }

    g_device = gpu_create_device(device_info);

    if (!g_device.is_valid())
    {
        return false;
    }

    add_plugin_observer(BEE_RENDER_MODULE_API_NAME, render_module_plugin_observer);
    return true;
}

void destroy_renderer()
{
    if (BEE_FAIL_F(g_device.is_valid(), "Renderer is already destroyed or uninitialized"))
    {
        return;
    }

    // Remove the observer first before destroying the device
    remove_plugin_observer(BEE_RENDER_MODULE_API_NAME, render_module_plugin_observer);

    for (auto* module : g_modules)
    {
        module->destroy_resources(g_device);
    }

    gpu_destroy_device(g_device);
    new (&g_device) DeviceHandle{};
}

void render_frame()
{
    for (int m = 0; m < g_modules.size(); ++m)
    {
        auto& module = g_modules[m];
//        auto& module_data = g_modules_data[m];

        module->execute(g_device);
    }
}


} // namespace bee


BEE_PLUGIN_API void load_plugin(bee::PluginRegistry* registry)
{
    bee::g_api.init = bee::init_renderer;
    bee::g_api.destroy = bee::destroy_renderer;
    bee::g_api.frame = bee::render_frame;

    registry->add_interface(BEE_RENDERER_API_NAME, &bee::g_api);
}

BEE_PLUGIN_API void unload_plugin(bee::PluginRegistry* registry)
{
    if (bee::g_device.is_valid())
    {
        bee::destroy_renderer();
    }

    registry->remove_interface(&bee::g_api);
}