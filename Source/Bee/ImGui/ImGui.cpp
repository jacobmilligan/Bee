/*
 *  ImGui.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#define BEE_IMGUI_GENERATOR_IMPLEMENTATION
#include "Bee/ImGui/ImGui.hpp"

#include "Bee/Core/Plugin.hpp"

#include "Bee/RenderGraph/RenderGraph.hpp"


namespace bee {


struct ImGuiPassData
{
    u8 pad;
};


static void* imgui_alloc_fun(size_t sz, void* user_data)
{
    return BEE_MALLOC(system_allocator(), sz);
}

static void imgui_free_func(void* ptr, void* user_data)
{
    if (ptr == nullptr)
    {
        return;
    }

    BEE_FREE(system_allocator(), ptr);
}

/*
 ***************************************************************************
 *
 * Override some of the generated functions to make working with the rest
 * of the engine easier
 *
 ***************************************************************************
 */
ImGuiContext* create_context(ImFontAtlas* shared_font_atlas)
{
    auto* ctx = igCreateContext(shared_font_atlas);
    igSetCurrentContext(ctx);

    auto* io = igGetIO();
    io->BackendPlatformName = "Bee.ImGui." BEE_OS_NAME_STRING;
//    io.BackendRendererName = "Bee.ImGui.Vulkan"; // TODO(Jacob): get backend name
//    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    igSetCurrentContext(nullptr);

    return ctx;
}

static void init_pass(const void* external_data, void* pass_data)
{

}

static void destroy_pass(const void* external_data, void* pass_data)
{

}

static void setup_pass(RenderGraphPass* pass, RenderGraphBuilderModule* builder, const void* external_data, void* pass_data)
{

}

static void execute_pass(
    RenderGraphPass* pass,
    RenderGraphStorage* storage,
    GpuCommandBackend* cmd,
    CommandBuffer* cmdbuf,
    const void* external_data,
    void* pass_data
)
{

}

RenderGraphPass* add_render_pass(RenderGraphModule* rg, RenderGraph* graph)
{
    return rg->add_pass<ImGuiPassData>(graph, "ImGui", setup_pass, execute_pass, init_pass, destroy_pass);
}


} // namespace bee

static bee::ImGuiModule g_imgui{};
static bee::ImGuiRenderModule g_render{};

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    bee_load_imgui_api(&g_imgui);
    g_imgui.SetAllocatorFunctions(bee::imgui_alloc_fun, bee::imgui_free_func, nullptr);
    // Override some of the generated functions to make working with the rest of the engine easier
    g_imgui.CreateContext = bee::create_context;

    loader->set_module(BEE_IMGUI_MODULE_NAME, &g_imgui, state);

    g_render.add_render_pass = bee::add_render_pass;
    loader->set_module(BEE_IMGUI_RENDER_MODULE, &g_render, state);
}

BEE_PLUGIN_VERSION(0, 0, 0);