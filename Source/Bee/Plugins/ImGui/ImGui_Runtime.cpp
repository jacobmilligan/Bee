/*
 *  ImGui.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Plugins/ImGui/ImGui.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Graphics/Shader.hpp"
#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/AssetPipeline/AssetPipeline.hpp"
#include "Bee/AssetPipeline/AssetDatabase.hpp"
#include "Bee/Plugins/Renderer/Renderer.hpp"
#include "Bee/Plugins/AssetRegistry/AssetRegistry.hpp"

#include <imgui.h>

namespace bee {


static Asset<Shader>        g_shader;
static TextureHandle        g_font;
static AssetRegistryApi*    g_asset_registry { nullptr };


void render_init(const DeviceHandle& device)
{
    g_shader = load_asset<Shader>(g_asset_registry, "shaders::ImGui");

    if (g_shader.status() == AssetStatus::invalid || g_shader.status() == AssetStatus::loading_failed)
    {
        return;
    }

    if (!g_font.is_valid())
    {
        auto& io = ImGui::GetIO();

        int font_width = 0;
        int font_height = 0;
        unsigned char* font_pixels = nullptr;
        io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height);

        TextureCreateInfo font_tex{};
        font_tex.type = TextureType::tex2d;
        font_tex.usage = TextureUsage::transfer_dst;
        font_tex.memory_usage = DeviceMemoryUsage::gpu_only;
        font_tex.width = sign_cast<u32>(font_width);
        font_tex.height = sign_cast<u32>(font_height);
        font_tex.debug_name = "ImGui font texture";

        g_font = gpu_create_texture(device, font_tex);
    }
}

void render_destroy(const DeviceHandle& device)
{
    g_shader.unload();

    gpu_destroy_texture(device, g_font);
    g_font = TextureHandle{};
}

void render_execute(const DeviceHandle& device)
{
    log_info("Executing!");
}


} // namespace bee


static bee::RenderModuleApi render_module{};


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry)
{
    ImGui::CreateContext();

    // Render module
    render_module.create_resources = bee::render_init;
    render_module.destroy_resources = bee::render_destroy;
    render_module.execute = bee::render_execute;
    registry->add_interface(BEE_RENDER_MODULE_API_NAME, &render_module);
}

BEE_PLUGIN_API void bee_unload_plugin(bee::PluginRegistry* registry)
{
    ImGui::DestroyContext();

    registry->remove_interface(&render_module);
}

BEE_REGISTER_PLUGIN(Bee.ImGuiPlugin);