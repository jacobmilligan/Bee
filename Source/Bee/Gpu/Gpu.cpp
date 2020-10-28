/*
 *  Gpu.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Plugins/Gpu/Gpu.hpp"
#include "Bee/Plugins/VulkanBackend/VulkanBackend.hpp"
#include "Bee/Core/Plugin.hpp"

namespace bee {

static GpuModule*       g_gpu = nullptr;
static PluginRegistry*  g_registry = nullptr;
static i32              g_current_backend = -1;

struct Backend
{
    GraphicsApi api { GraphicsApi::none };
    const char* plugin_name { nullptr };
};

static constexpr Backend g_backends[] = {
    { GraphicsApi::none, nullptr },
    { GraphicsApi::vulkan, "Bee.VulkanBackend" }
};

i32 enumerate_available_backends(GraphicsApi* dst)
{
    if (dst != nullptr)
    {
        dst[0] = GraphicsApi::vulkan;
    }

    return 1;
}

GpuModule* load_backend(const GraphicsApi api)
{
    const i32 index = find_index_if(g_backends, [&](const Backend& backend) { return backend.api == api; });
    BEE_ASSERT(index >= 0);

    const auto& new_backend = g_backends[index];
    const auto& old_backend = g_backends[g_current_backend];

    if (g_gpu != nullptr)
    {
        g_gpu->destroy();
        g_gpu = nullptr;
        g_registry->unload_plugin(old_backend.plugin_name);
    }

    if (new_backend.plugin_name != nullptr)
    {
        g_registry->load_plugin(new_backend.plugin_name, PluginVersion{});
        g_gpu = g_registry->get_module<GpuModule>(BEE_GPU_MODULE_NAME);
        if (g_gpu == nullptr || !g_gpu->init())
        {
            g_registry->unload_plugin(new_backend.plugin_name);
        }
    }

    g_current_backend = index;

    return g_gpu;
}

GraphicsApi current_backend()
{
    if (g_current_backend < 0)
    {
        return GraphicsApi::none;
    }
    return g_backends[g_current_backend].api;
}


} // namespace bee

static bee::GpuSetupModule g_setup{};

BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    bee::g_registry = registry;
    g_setup.enumerate_available_backends = bee::enumerate_available_backends;
    g_setup.load_backend = bee::load_backend;
    g_setup.current_backend = bee::current_backend;
    registry->toggle_module(state, BEE_GPU_SETUP_MODULE_NAME, &g_setup);
}