/*
 *  ImGui.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#define BEE_IMGUI_GENERATOR_IMPLEMENTATION
#include "Bee/ImGui/ImGui.hpp"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Filesystem.hpp"

#include "Bee/RenderGraph/RenderGraph.hpp"
#include "Bee/AssetPipeline/AssetPipeline.hpp"
#include "Bee/ShaderPipeline/Cache.hpp"


namespace bee {


struct ImGuiRender
{
    Allocator*              allocator { nullptr };
    DeviceHandle            device;
    ShaderPipelineHandle    shader;
    ShaderCache*            shader_cache { nullptr };
    GpuBackend*             gpu { nullptr };
    TextureHandle           font_texture;
    TextureViewHandle       font_texture_view;
    BufferHandle            vertex_buffer;
    BufferHandle            index_buffer;
    i32                     vertex_buffer_size { -1 };
    i32                     index_buffer_size { -1 };
    PipelineStateDescriptor pipeline_desc;
};

static ShaderCacheModule*   g_shader_cache = nullptr;
static GpuBackend*          g_gpu = nullptr;

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

Result<ImGuiRender*, ImGuiError> create_render(const DeviceHandle device, GpuBackend* gpu, ShaderCache* shader_cache, Allocator* allocator)
{
    const auto shader = g_shader_cache->lookup_shader_by_name(shader_cache, "ImGuiPipeline");

    if (!shader.is_valid())
    {
        return { ImGuiError::missing_shader };
    }

    if (!g_shader_cache->load_shader(shader_cache, shader))
    {
        return { ImGuiError::failed_to_load_shader };
    }

    auto* imio = igGetIO();

    unsigned char* font_pixels = nullptr;
    int font_width = -1;
    int font_height = -1;
    int font_bpp = -1;
    ImFontAtlas_GetTexDataAsAlpha8(imio->Fonts, &font_pixels, &font_width, &font_height, &font_bpp);

    TextureCreateInfo font_info{};
    font_info.type = TextureType::tex2d;
    font_info.usage = TextureUsage::transfer_dst;
    font_info.format = PixelFormat::a8;
    font_info.memory_usage = DeviceMemoryUsage::gpu_only;
    font_info.width = sign_cast<u32>(font_width);
    font_info.height = sign_cast<u32>(font_height);
    font_info.debug_name = "Bee.ImGui.Font";

    const auto font_texture = gpu->create_texture(device, font_info);

    if (!font_texture.is_valid())
    {
        g_shader_cache->unload_shader(shader_cache, shader);
        return { ImGuiError::failed_to_create_font_texture };
    }

    Extent font_extent(font_info.width, font_info.height);
    g_gpu->update_texture(device, font_texture, font_pixels, Offset{}, font_extent, 0, 0);

    auto* render = BEE_NEW(allocator, ImGuiRender);
    render->allocator = allocator;
    render->device = device;
    render->gpu = gpu;
    render->shader_cache = shader_cache;
    render->shader = shader;
    render->font_texture = font_texture;
    render->font_texture_view = gpu->create_texture_view_from(device, font_texture);

    return render;
}

Result<void, ImGuiError> destroy_render(ImGuiRender* render)
{
    if (!g_shader_cache->unload_shader(render->shader_cache, render->shader))
    {
        return { ImGuiError::failed_to_load_shader };
    }

    if (render->vertex_buffer.is_valid())
    {
        render->gpu->destroy_buffer(render->device, render->vertex_buffer);
    }

    if (render->index_buffer.is_valid())
    {
        render->gpu->destroy_buffer(render->device, render->index_buffer);
    }

    render->gpu->destroy_texture_view(render->device, render->font_texture_view);
    render->gpu->destroy_texture(render->device, render->font_texture);
    BEE_DELETE(render->allocator, render);
    return {};
}

void draw(ImGuiRender* render, CommandBuffer* cmd_buf)
{
    auto* draw_data = igGetDrawData();
    if (draw_data == nullptr)
    {
        return;
    }

    auto& vertex_buffer = render->vertex_buffer;
    auto& index_buffer = render->index_buffer;
    const i32 new_vertex_buffer_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    const i32 new_index_buffer_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

    BufferCreateInfo info{};

    // Create or resize buffers if needed
    if (render->vertex_buffer_size < new_vertex_buffer_size)
    {
        if (vertex_buffer.is_valid())
        {
            g_gpu->destroy_buffer(render->device, vertex_buffer);
        }

        info.size = new_vertex_buffer_size;
        info.type = BufferType::vertex_buffer | BufferType::transfer_dst;
        info.debug_name = "Bee.ImGui.VertexBuffer";
        info.memory_usage = DeviceMemoryUsage::gpu_only;
        render->vertex_buffer = g_gpu->create_buffer(render->device, info);
    }

    if (render->index_buffer_size < new_index_buffer_size)
    {
        if (index_buffer.is_valid())
        {
            g_gpu->destroy_buffer(render->device, index_buffer);
        }

        info.size = new_index_buffer_size;
        info.type = BufferType::index_buffer | BufferType::transfer_dst;
        info.debug_name = "Bee.ImGui.VertexBuffer";
        info.memory_usage = DeviceMemoryUsage::gpu_only;
        render->index_buffer = g_gpu->create_buffer(render->device, info);
    }

    // Upload buffer draw data
    int vtx_offset = 0;
    int idx_offset = 0;
    for (int i = 0; i < draw_data->CmdListsCount; ++i)
    {
        auto* cmd_list = draw_data->CmdLists[i];
        const i32 vtx_size = cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
        const i32 idx_size = cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);

        g_gpu->update_buffer(render->device, render->vertex_buffer, cmd_list->VtxBuffer.Data, vtx_offset, vtx_size);
        g_gpu->update_buffer(render->device, render->index_buffer, cmd_list->IdxBuffer.Data, idx_offset, idx_size);

        vtx_offset += vtx_size;
        idx_offset += idx_size;
    }

    render->vertex_buffer_size = new_vertex_buffer_size;
    render->index_buffer_size = new_index_buffer_size;

    auto* cmd = render->gpu->get_command_backend();
    cmd->bind_vertex_buffer(cmd_buf, render->vertex_buffer, 0, 0);
    cmd->bind_index_buffer(cmd_buf, render->index_buffer, 0, IndexFormat::uint16);

    TextureBindingUpdate texture_update(render->font_texture_view);
    ResourceBindingUpdate update(1, 0, 1, &texture_update);
    g_shader_cache->update_resources(render->shader_cache, render->shader, 0, 1, &update);
    g_shader_cache->bind_resources(render->shader_cache, render->shader, cmd_buf);

    // Draw out the command lists but keep track of vtx/idx count externally because all the
    // separate cmd list data is now in two big buffers instead of several per-cmdlist buffers
    vtx_offset = 0;
    idx_offset = 0;
    for (int i = 0; i < draw_data->CmdListsCount; ++i)
    {
        auto* cmd_list = draw_data->CmdLists[i];

        for (int c = 0; c < cmd_list->CmdBuffer.Size; ++c)
        {
            auto& imgui_cmd = cmd_list->CmdBuffer.Data[c];
            cmd->draw_indexed(
                cmd_buf,
                g_shader_cache->get_pipeline_desc(render->shader_cache, render->shader),
                imgui_cmd.ElemCount,
                1,
                imgui_cmd.VtxOffset + vtx_offset,
                imgui_cmd.IdxOffset + idx_offset,
                0
            );
        }

        vtx_offset += cmd_list->VtxBuffer.Size;
        idx_offset += cmd_list->IdxBuffer.Size;
    }
}


} // namespace bee

static bee::ImGuiModule         g_imgui{};
static bee::ImGuiRenderModule   g_render{};

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    if (!loader->require_plugin("Bee.ShaderPipeline", { 0, 0, 0 }))
    {
        return;
    }

    if (loader->is_plugin_loaded("Bee.AssetPipeline", {0, 0, 0}))
    {
        auto* ap = static_cast<bee::AssetPipelineModule*>(loader->get_module(BEE_ASSET_PIPELINE_MODULE_NAME));
        ap->watch_external_sources(bee::fs::roots().sources.join("ImGui/Assets"), state == bee::PluginState::loading);
    }

    bee_load_imgui_api(&g_imgui);
    g_imgui.SetAllocatorFunctions(bee::imgui_alloc_fun, bee::imgui_free_func, nullptr);
    // Override some of the generated functions to make working with the rest of the engine easier
    g_imgui.CreateContext = bee::create_context;
    loader->set_module(BEE_IMGUI_MODULE_NAME, &g_imgui, state);

    // Render module
    g_render.create = bee::create_render;
    g_render.destroy = bee::destroy_render;
    g_render.draw = bee::draw;
    loader->set_module(BEE_IMGUI_RENDER_MODULE_NAME, &g_render, state);

    bee::g_shader_cache = static_cast<bee::ShaderCacheModule*>(loader->get_module(BEE_SHADER_CACHE_MODULE_NAME));
}

BEE_PLUGIN_VERSION(0, 0, 0);