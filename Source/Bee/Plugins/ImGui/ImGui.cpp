/*
 *  ImGui.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee.ImGui.Descriptor.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Plugins/ImGui/ImGui.hpp"
#include "Bee/Plugins/Renderer/Renderer.hpp"
#include "Bee/Plugins/AssetPipeline/AssetPipeline.hpp"

#include <imgui.h>


namespace bee {


struct ImGuiData
{
    ImGuiContext* ctx { nullptr };
};

static ImGuiData*           g_data { nullptr };
static bee::ImGuiModule     g_imgui_module{};
static bee::RenderStage     g_render_stage{};


void init_render_stage(const DeviceHandle& device, RenderStageData* ctx)
{

}

void destroy_render_stage(const DeviceHandle& device, RenderStageData* ctx)
{

}

void execute_render_stage(const DeviceHandle& device, RenderStageData* ctx)
{

}

} // namespace bee


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    bee::g_data = registry->get_or_create_persistent<bee::ImGuiData>("BeeImGuiData");

    bee::g_render_stage.init = bee::init_render_stage;
    bee::g_render_stage.destroy = bee::destroy_render_stage;
    bee::g_render_stage.execute = bee::execute_render_stage;

    registry->toggle_module(state, BEE_IMGUI_MODULE_NAME, &bee::g_imgui_module);

    auto* renderer = registry->get_module<bee::RendererModule>(BEE_RENDERER_MODULE_NAME);

    if (state == bee::PluginState::loading)
    {
        renderer->add_stage(&bee::g_render_stage);
    }
    else
    {
        renderer->remove_stage(&bee::g_render_stage);
    }
}