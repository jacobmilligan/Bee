/*
 *  Renderer.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Plugins/Renderer/Renderer.hpp"
#include "Bee/Core/Plugin.hpp"

#include <algorithm>


namespace bee {


static constexpr i32 max_swapchains = 32;
static constexpr i32 max_graphs = 32;

struct RegisteredSwapchain
{
    SwapchainKind   kind { SwapchainKind::secondary };
    i32             id { 0 };
    u32             hash { 0 };
    const char*     name { nullptr };
    SwapchainHandle handle;
};

struct RenderGraph
{
    DeviceHandle    device;
};

struct Renderer
{
    DeviceHandle                device;
    DynamicArray<RenderStage*>  stages;
    i32                         swapchain_count { 0 };
    RegisteredSwapchain         swapchains[max_swapchains];
    i32                         graph_count { 0 };
    RenderGraph                 graphs[max_graphs];
    RenderGraph*                default_graph { nullptr };
};

static Renderer*            g_renderer { nullptr };

/*
 *********************************
 *
 * Render graph API
 *
 *********************************
 */
static RenderGraphBuilderModule   g_builder{};

RenderGraph* rg_create_render_graph(const DeviceHandle device)
{
    if (BEE_FAIL_F(g_renderer->graph_count < max_graphs, "Cannot create more than max_graphs (%d) render graphs", max_graphs))
    {
        return nullptr;
    }

    auto* graph = &g_renderer->graphs[g_renderer->graph_count++];
    graph->device = device;
    return graph;
}

void rg_destroy_render_graph(RenderGraph* graph)
{

}


RenderGraphResource rg_create_buffer(RenderGraphPass* pass, const char* name, const BufferCreateInfo& create_info)
{

}

RenderGraphResource rg_create_texture(RenderGraphPass* pass, const char* name, const TextureCreateInfo& create_info)
{

}

void rg_write_color(RenderGraphPass* pass, const RenderGraphResource& texture, const LoadOp load, const StoreOp store)
{

}

void rg_write_depth(RenderGraphPass* pass, const RenderGraphResource& texture, const PixelFormat depth_format, const LoadOp load, const StoreOp store)
{

}

void rg_set_execute_function(RenderGraphPass* pass, render_graph_execute_t fn)
{

}

RenderGraphPass* rg_add_dynamic_pass(RenderGraph* graph, const char* name, void** args, const size_t args_size)
{

}

DeviceHandle rg_get_device()
{
    return g_renderer->device;
}

i32 rg_get_swapchains(SwapchainHandle* dst)
{
    if (dst != nullptr)
    {
        for (int i = 0; i < g_renderer->swapchain_count; ++i)
        {
            dst[i] = g_renderer->swapchains[i].handle;
        }
    }

    return g_renderer->swapchain_count;
}

SwapchainHandle rg_get_primary_swapchain()
{
    return g_renderer->swapchain_count > 0 ? g_renderer->swapchains[0].handle : SwapchainHandle{};
}

/*
 *********************************
 *
 * Renderer module implementation
 *
 *********************************
 */
bool operator<(const RegisteredSwapchain& lhs, const RegisteredSwapchain& rhs)
{
    if (lhs.kind != rhs.kind)
    {
        return lhs.kind < rhs.kind;
    }

    if (lhs.id != rhs.id)
    {
        // we want the last-created swapchains to be at the front of the list
        return lhs.id > rhs.id;
    }

    return lhs.hash < rhs.hash;
}

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

    // initialize the default render graph
    g_renderer->default_graph = rg_create_render_graph(g_renderer->device);

    if (BEE_FAIL(g_renderer->default_graph != nullptr))
    {
        return false;
    }

    // Initialize any render stages added before the renderer was initialized
    for (auto& stage : g_renderer->stages)
    {
        stage->init(g_renderer->device);
    }

    return true;
}

void destroy_renderer()
{
    if (BEE_FAIL_F(g_renderer->device.is_valid(), "Renderer is already destroyed or uninitialized"))
    {
        return;
    }

    for (int i = 0; i < g_renderer->graph_count; ++i)
    {
        rg_destroy_render_graph(&g_renderer->graphs[i]);
    }

    g_renderer->graph_count = 0;
    g_renderer->default_graph = nullptr;

    for (auto* stage : g_renderer->stages)
    {
        stage->destroy(g_renderer->device);
    }

    g_renderer->stages.clear();

    for (int i = 0; i < g_renderer->swapchain_count; ++i)
    {
        gpu_destroy_swapchain(g_renderer->device, g_renderer->swapchains[i].handle);
    }

    gpu_destroy_device(g_renderer->device);
    g_renderer->device = DeviceHandle{};
    g_renderer->swapchain_count = 0;
}

void execute_frame()
{
    for (auto* stage : g_renderer->stages)
    {
        stage->execute(&g_builder, );
    }
}

void add_stage(RenderStage* stage)
{
    const auto index = find_index(g_renderer->stages, stage);

    if (index >= 0)
    {
        g_renderer->stages[index] = stage;
    }
    else
    {
        g_renderer->stages.push_back(stage);

        if (g_renderer->device.is_valid())
        {
            stage->init(g_renderer->device);
        }
    }
}

void remove_stage(RenderStage* stage)
{
    const auto index = find_index(g_renderer->stages, stage);

    if (index >= 0)
    {
        if (g_renderer->device.is_valid())
        {
            g_renderer->stages[index]->destroy(g_renderer->device);
        }

        g_renderer->stages.erase(index);
    }
}

DeviceHandle get_renderer_device()
{
    return g_renderer->device;
}

void add_renderer_swapchain(const SwapchainKind kind, const WindowHandle& window, const PixelFormat format, const char* name)
{
    if (BEE_FAIL_F(g_renderer->swapchain_count < max_swapchains, "Cannot add more than max_swapchains (%d) to renderer", max_swapchains))
    {
        return;
    }

    SwapchainCreateInfo create_info{};
    create_info.texture_format = format;
    create_info.texture_extent = Extent::from_platform_size(get_window_framebuffer_size(window));
    create_info.texture_usage = TextureUsage::color_attachment;
    create_info.texture_array_layers = 1;
    create_info.vsync = true;
    create_info.window = window;
    create_info.debug_name = name;

    const auto handle = gpu_create_swapchain(g_renderer->device, create_info);

    if (!handle.is_valid())
    {
        log_error("Failed to add swapchain to renderer");
        return;
    }

    const auto name_hash = get_hash(name);
    const auto index = g_renderer->swapchain_count;
    ++g_renderer->swapchain_count;

    auto& swapchain = g_renderer->swapchains[index];
    swapchain.id = index;
    swapchain.kind = kind;
    swapchain.handle = handle;
    swapchain.name = name;
    swapchain.hash = name_hash;

    if (kind == SwapchainKind::primary)
    {
        std::sort(g_renderer->swapchains, g_renderer->swapchains + g_renderer->swapchain_count);
    }
}

void remove_renderer_swapchain(const char* name)
{
    const auto hash = get_hash(name);
    const auto index = find_index_if(
        g_renderer->swapchains,
        g_renderer->swapchains + g_renderer->swapchain_count,
        [&](const RegisteredSwapchain& s)
        {
            return s.hash == hash;
        }
    );

    if (index < 0)
    {
        log_error("Swapchain \"%s\" was not added to the renderer", name);
        return;
    }

    gpu_destroy_swapchain(g_renderer->device, g_renderer->swapchains[index].handle);

    // swap the old swapchain to the back, decrement size
    std::swap(g_renderer->swapchains[index], g_renderer->swapchains[g_renderer->swapchain_count - 1]);
    --g_renderer->swapchain_count;

    // re-sort the swapchains
    std::sort(g_renderer->swapchains, g_renderer->swapchains + g_renderer->swapchain_count);
}

i32 get_renderer_swapchains(RegisteredSwapchain* swapchains)
{
    if (swapchains == nullptr)
    {
        return g_renderer->swapchain_count;
    }

    memcpy(swapchains, g_renderer->swapchains, sizeof(RegisteredSwapchain) * g_renderer->swapchain_count);
    return g_renderer->swapchain_count;
}

RegisteredSwapchain* get_primary_renderer_swapchain()
{
    return g_renderer->swapchain_count > 0 ? nullptr : &g_renderer->swapchains[0];
}


} // namespace bee


static bee::RendererModule      g_module{};


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    bee::g_renderer = registry->get_or_create_persistent<bee::Renderer>("BeeRenderer");

    // Renderer
    g_module.init = bee::init_renderer;
    g_module.destroy = bee::destroy_renderer;
    g_module.execute_frame = bee::execute_frame;
    g_module.add_stage = bee::add_stage;
    g_module.remove_stage = bee::remove_stage;
    g_module.get_device = bee::get_renderer_device;
    g_module.add_swapchain = bee::add_renderer_swapchain;
    g_module.remove_swapchain = bee::remove_renderer_swapchain;
    registry->toggle_module(state, BEE_RENDERER_MODULE_NAME, &g_module);

    // RenderGraphBuilder
    bee::g_builder.create_render_graph = bee::rg_create_render_graph;
    bee::g_builder.destroy_render_graph = bee::rg_destroy_render_graph;
    bee::g_builder.create_buffer = bee::rg_create_buffer;
    bee::g_builder.create_texture = bee::rg_create_texture;
    bee::g_builder.write_color = bee::rg_write_color;
    bee::g_builder.write_depth = bee::rg_write_depth;
    bee::g_builder.set_execute_function = bee::rg_set_execute_function;
    bee::g_builder.add_dynamic_pass = bee::rg_add_dynamic_pass;
    registry->toggle_module(state, BEE_RENDER_GRAPH_BUILDER_MODULE_NAME, &bee::g_builder);
}