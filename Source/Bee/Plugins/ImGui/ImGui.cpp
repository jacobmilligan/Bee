/*
 *  ImGui.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee.ImGui.Descriptor.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Plugins/ImGui/ImGui.hpp"
#include "Bee/Plugins/ShaderPipeline/Shader.hpp"
#include "Bee/Plugins/Renderer/Renderer.hpp"
#include "Bee/Plugins/AssetPipeline/AssetPipeline.hpp"
#include "Bee/Plugins/AssetRegistry/AssetRegistry.hpp"

#include <imgui.h>


namespace bee {


struct ImGuiData
{
    ImGuiContext*   ctx { nullptr };
    RenderStage     render_stage;
    Asset<Shader>   shader;
};

static ImGuiData*           g_imgui { nullptr };
static AssetRegistryModule* g_asset_registry { nullptr };
static RendererModule*      g_renderer { nullptr };


void init_render_stage(const DeviceHandle& device, RenderStageData* ctx)
{
    BEE_ASSERT(g_asset_registry->get_manifest != nullptr);

    auto* manifest = g_asset_registry->get_manifest("ImGui");
    const auto guid = manifest->get("Shader");
    BEE_ASSERT(guid != invalid_guid);
    g_imgui->shader = g_asset_registry->load_asset<Shader>(guid, device);
}

void destroy_render_stage(const DeviceHandle& device, RenderStageData* ctx)
{

}

void execute_render_stage(const DeviceHandle& device, RenderStageData* ctx)
{

}

void init()
{
    g_renderer->add_stage(&g_imgui->render_stage);
}

void destroy()
{
    g_renderer->remove_stage(&g_imgui->render_stage);
}


} // namespace bee


static bee::ImGuiModule g_module{};


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    bee::g_asset_registry = registry->get_module<bee::AssetRegistryModule>(BEE_ASSET_REGISTRY_MODULE_NAME);
    bee::g_renderer = registry->get_module<bee::RendererModule>(BEE_RENDERER_MODULE_NAME);

    bee::g_imgui = registry->get_or_create_persistent<bee::ImGuiData>("BeeImGuiData");
    g_module.init = bee::init;
    g_module.destroy = bee::destroy;

    auto& render_stage = bee::g_imgui->render_stage;
    render_stage.init = bee::init_render_stage;
    render_stage.destroy = bee::destroy_render_stage;
    render_stage.execute = bee::execute_render_stage;

    registry->toggle_module(state, BEE_IMGUI_MODULE_NAME, &g_module);
}