/*
 *  Renderer.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Plugins/Renderer/Renderer.hpp"
#include "Bee/Core/Plugin.hpp"

namespace bee {


struct Renderer
{
    DeviceHandle                device;
    DynamicArray<RenderStage*>  stages;
    DynamicArray<u32>           stage_ids;
};

static Renderer* g_renderer { nullptr };


bool init_renderer(const DeviceCreateInfo& device_info)
{
    if (g_renderer->device.is_valid())
    {
        log_error("Renderer is already initialized");
        return false;
    }

    g_renderer->device = gpu_create_device(device_info);

    if (BEE_FAIL(g_renderer->device.is_valid()))
    {
        return false;
    }

    return true;
}

void destroy_renderer()
{
    if (BEE_FAIL_F(g_renderer->device.is_valid(), "Renderer is already destroyed or uninitialized"))
    {
        return;
    }

    for (auto* stage : g_renderer->stages)
    {
        stage->destroy(g_renderer->device, stage->data);
    }

    gpu_destroy_device(g_renderer->device);
    g_renderer->device = DeviceHandle{};
}

void render_frame()
{
    for (auto* stage : g_renderer->stages)
    {
        stage->execute(g_renderer->device, stage->data);
    }
}

void add_stage(const u32 id, RenderStage* stage)
{
    const auto index = container_find_index(g_renderer->stage_ids, id);

    if (index >= 0)
    {
        g_renderer->stages[index] = stage;
    }
    else
    {
        g_renderer->stages.push_back(stage);
        g_renderer->stage_ids.push_back(id);
    }
}

void remove_stage(RenderStage* stage)
{
    const auto index = container_find_index(g_renderer->stages, stage);

    if (index >= 0)
    {
        g_renderer->stages.erase(index);
        g_renderer->stage_ids.erase(index);
    }
}


} // namespace bee


static bee::RendererModule g_renderer_module{};


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    bee::g_renderer = registry->get_or_create_persistent<bee::Renderer>("BeeRenderer");

    g_renderer_module.init = bee::init_renderer;
    g_renderer_module.destroy = bee::destroy_renderer;
    g_renderer_module.frame = bee::render_frame;
    g_renderer_module.add_stage = bee::add_stage;
    g_renderer_module.remove_stage = bee::remove_stage;

    registry->toggle_module(state, BEE_RENDERER_MODULE_NAME, &g_renderer_module);
}