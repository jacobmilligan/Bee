/*
 *  ImGui.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/Plugins/ImGui/ImGui.hpp"
#include "Bee/Plugins/ShaderPipeline/Material.hpp"
#include "Bee/Plugins/Renderer/Renderer.hpp"
#include "Bee/Plugins/AssetPipeline/AssetPipeline.hpp"
#include "Bee/Plugins/AssetRegistry/AssetRegistry.hpp"

#include <imgui.h>


namespace bee {


struct RenderStageData
{
    ImGuiContext*   ctx { nullptr };
    Asset<Material> material;
};

struct RenderGraphArgs
{
    RenderGraphResource target;
};

static RenderStageData*     g_imgui { nullptr };
static AssetRegistryModule* g_asset_registry { nullptr };


void init_render_stage(const DeviceHandle& device)
{
    BEE_ASSERT(g_asset_registry->get_manifest != nullptr);

    auto* manifest = g_asset_registry->get_manifest("ImGui");
    BEE_ASSERT(manifest != nullptr);
    g_imgui->material = manifest->load<Material>(g_asset_registry, "Material", device);
}

void destroy_render_stage(const DeviceHandle& device)
{
    g_imgui->material.unload();
}

void execute_render_stage(RenderGraph* graph, RenderGraphBuilderModule* builder)
{
    auto* pass = builder->add_pass(graph, "ImGui");
    const auto target = builder->import_backbuffer(pass, "Backbuffer", builder->get_primary_swapchain());
    builder->write_color(pass, target, LoadOp::clear, StoreOp::store, 1);

    builder->set_execute(pass, [](RenderGraph* graph, RenderGraphStorage* storage)
    {
        auto* cmd = storage->create_command_buffer(graph, QueueType::graphics);
        BEE_UNUSED(cmd);
    });
}

void init_imgui()
{

}

void destroy_imgui()
{

}


} // namespace bee


static bee::ImGuiModule g_module{};
static bee::RenderStage g_stage{};


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    bee::g_imgui = registry->get_or_create_persistent<bee::RenderStageData>("BeeImGuiData");
    bee::g_asset_registry = registry->get_module<bee::AssetRegistryModule>(BEE_ASSET_REGISTRY_MODULE_NAME);

    g_module.init = bee::init_imgui;
    g_module.destroy = bee::destroy_imgui;

    g_stage.init = bee::init_render_stage;
    g_stage.destroy = bee::destroy_render_stage;
    g_stage.execute = bee::execute_render_stage;

    registry->toggle_module(state, BEE_IMGUI_MODULE_NAME, &g_module);

    auto* renderer = registry->get_module<bee::RendererModule>(BEE_RENDERER_MODULE_NAME);

    if (state == bee::PluginState::loading)
    {
        renderer->add_stage(&g_stage);
    }
    else
    {
        renderer->remove_stage(&g_stage);
    }
}