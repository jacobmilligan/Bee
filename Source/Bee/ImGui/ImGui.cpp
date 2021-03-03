/*
 *  ImGui.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#define BEE_IMGUI_INTERNAL
#include "Bee/ImGui/ImGui.hpp"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Filesystem.hpp"

#include "Bee/RenderGraph/RenderGraph.hpp"
#include "Bee/AssetPipeline/AssetPipeline.hpp"
#include "Bee/ShaderPipeline/ShaderPipeline.hpp"
#include "Bee/Input/Input.hpp"
#include "Bee/Input/Mouse.hpp"
#include "Bee/Input/Keyboard.hpp"

BEE_PUSH_WARNING
    BEE_DISABLE_PADDING_WARNINGS
    #include <imgui/imgui.h>
BEE_POP_WARNING

namespace bee {


struct ImGuiBackend
{
    Allocator*              allocator { nullptr };
    Path                    assets_path;
    ImGuiContext*           ctx { nullptr };
    DeviceHandle            device;
    BEE_PAD(4);
    Asset<Shader>           shader;
    AssetPipeline*          asset_pipeline { nullptr };
    GpuBackend*             gpu { nullptr };
    TextureHandle           font_texture;
    TextureViewHandle       font_texture_view;
    BufferHandle            vertex_buffer;
    BufferHandle            index_buffer;
    PipelineStateDescriptor pipeline_desc;
    u64                     time { 0 };
};

static ShaderPipelineModule*    g_shader_pipeline = nullptr;
static PlatformModule*          g_platform = nullptr;
static AssetPipelineModule*     g_asset_pipeline = nullptr;
static InputModule*             g_input = nullptr;

/*
 ***************************************************************************
 *
 * Override some of the generated functions to make working with the rest
 * of the engine easier
 *
 ***************************************************************************
 */
Result<ImGuiBackend*, ImGuiError> create_backend(const DeviceHandle device, GpuBackend* gpu, AssetPipeline* asset_pipeline, Allocator* allocator)
{
    Path assets_path(get_plugin_source_path("Bee.ImGui"));
    if (!assets_path.empty())
    {
        assets_path.append("Assets");
        g_asset_pipeline->add_import_root(asset_pipeline, assets_path.view());
    }

    auto res = g_asset_pipeline->load_asset<Shader>(asset_pipeline, "ImGui.ImGuiPipeline");

    if (!res)
    {
        if (res.unwrap_error() == AssetPipelineError::failed_to_locate)
        {
            return { ImGuiError::missing_shader };
        }
        return { ImGuiError::failed_to_load_shader };
    }

    auto shader = BEE_MOVE(res.unwrap());
    shader->pipeline_desc.vertex_description.attributes[2].format = VertexFormat::unormbyte4;
    shader->pipeline_desc.vertex_description.layouts[0].stride = sizeof(ImDrawVert);

    auto* ctx = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.BackendPlatformName = "Bee.ImGui." BEE_OS_NAME_STRING;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    unsigned char* font_pixels = nullptr;
    int font_width = -1;
    int font_height = -1;
    int font_bpp = -1;
    io.Fonts->GetTexDataAsRGBA32(&font_pixels, &font_width, &font_height, &font_bpp);

    TextureCreateInfo font_info{};
    font_info.type = TextureType::tex2d;
    font_info.usage = TextureUsage::transfer_dst | TextureUsage::sampled;
    font_info.format = PixelFormat::rgba8;
    font_info.sample_count = 1;
    font_info.memory_usage = DeviceMemoryUsage::gpu_only;
    font_info.width = sign_cast<u32>(font_width);
    font_info.height = sign_cast<u32>(font_height);
    font_info.debug_name = "Bee.ImGui.Font";

    const auto font_texture = gpu->create_texture(device, font_info);

    if (!font_texture.is_valid())
    {
        ImGui::DestroyContext(ctx);
        return { ImGuiError::failed_to_create_font_texture };
    }

    Extent font_extent(font_info.width, font_info.height);
    gpu->update_texture(device, font_texture, font_pixels, Offset{}, font_extent, 0, 0);

    auto* backend = BEE_NEW(allocator, ImGuiBackend);
    backend->allocator = allocator;
    backend->ctx = ctx;
    backend->device = device;
    backend->gpu = gpu;
    backend->asset_pipeline = asset_pipeline;
    backend->shader = BEE_MOVE(shader);
    backend->font_texture = font_texture;
    backend->font_texture_view = gpu->create_texture_view_from(device, font_texture);
    backend->assets_path = BEE_MOVE(assets_path);

    TextureBindingUpdate texture_update(backend->font_texture_view);
    ResourceBindingUpdate update(0, 0, 1, &texture_update);
    g_shader_pipeline->update_resources(backend->shader, 0, 1, &update);

    return backend;
}

Result<void, ImGuiError> destroy_backend(ImGuiBackend* backend)
{
    if (!backend->assets_path.empty())
    {
        g_asset_pipeline->remove_import_root(backend->asset_pipeline, backend->assets_path.view());
    }

    if (!backend->shader.unload())
    {
        return { ImGuiError::failed_to_load_shader };
    }

    if (backend->vertex_buffer.is_valid())
    {
        backend->gpu->destroy_buffer(backend->device, backend->vertex_buffer);
    }

    if (backend->index_buffer.is_valid())
    {
        backend->gpu->destroy_buffer(backend->device, backend->index_buffer);
    }

    backend->gpu->destroy_texture_view(backend->device, backend->font_texture_view);
    backend->gpu->destroy_texture(backend->device, backend->font_texture);

    ImGui::DestroyContext(backend->ctx);

    BEE_DELETE(backend->allocator, backend);
    return {};
}

void draw(ImGuiBackend* backend, CommandBuffer* cmd_buf)
{
    auto* draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr)
    {
        return;
    }

    auto* cmd = backend->gpu->get_command_backend();
    const i32 new_vertex_buffer_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    const i32 new_index_buffer_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

    BufferCreateInfo info{};

    // Create or resize buffers if needed
    if (new_vertex_buffer_size > 0 && !backend->vertex_buffer.is_valid())
    {
        info.size = new_vertex_buffer_size;
        info.type = BufferType::vertex_buffer | BufferType::dynamic_buffer;
        info.debug_name = "Bee.ImGui.VertexBuffer";
        info.memory_usage = DeviceMemoryUsage::cpu_to_gpu;
        backend->vertex_buffer = backend->gpu->create_buffer(backend->device, info);
    }

    if (new_index_buffer_size > 0 && !backend->index_buffer.is_valid())
    {
        info.size = new_index_buffer_size;
        info.type = BufferType::index_buffer | BufferType::dynamic_buffer;
        info.debug_name = "Bee.ImGui.VertexBuffer";
        info.memory_usage = DeviceMemoryUsage::cpu_to_gpu;
        backend->index_buffer = backend->gpu->create_buffer(backend->device, info);
    }

    const auto off = IM_OFFSETOF(ImDrawVert, uv);
    BEE_UNUSED(off);


    // Upload buffer draw data
    int vtx_offset = 0;
    int idx_offset = 0;
    for (int i = 0; i < draw_data->CmdListsCount; ++i)
    {
        auto* cmd_list = draw_data->CmdLists[i];
        const i32 vtx_size = cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
        const i32 idx_size = cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);

        backend->gpu->update_buffer(backend->device, backend->vertex_buffer, cmd_list->VtxBuffer.Data, vtx_offset, vtx_size);
        backend->gpu->update_buffer(backend->device, backend->index_buffer, cmd_list->IdxBuffer.Data, idx_offset, idx_size);

        vtx_offset += vtx_size;
        idx_offset += idx_size;
    }

    if (draw_data->TotalVtxCount > 0)
    {
        cmd->bind_vertex_buffer(cmd_buf, backend->vertex_buffer, 0, 0);
    }
    if (draw_data->TotalIdxCount > 0)
    {
        cmd->bind_index_buffer(cmd_buf, backend->index_buffer, 0, sizeof(ImDrawIdx) == 2 ? IndexFormat::uint16 : IndexFormat::uint32);
    }

    g_shader_pipeline->bind_resources(backend->shader, cmd_buf);

    struct PushConstant
    {
        float2 scale;
        float2 translate;
    } push_constant;

    push_constant.scale.x = 2.0f / draw_data->DisplaySize.x;
    push_constant.scale.y = 2.0f / draw_data->DisplaySize.y;
    push_constant.translate.x = -1.0f - draw_data->DisplayPos.x * push_constant.scale[0];
    push_constant.translate.y = -1.0f - draw_data->DisplayPos.y * push_constant.scale[1];

    cmd->push_constants(cmd_buf, 0, &push_constant);

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

            RenderRect scissor(
                sign_cast<i32>((imgui_cmd.ClipRect.x - draw_data->DisplayPos.x) * draw_data->FramebufferScale.x),
                sign_cast<i32>((imgui_cmd.ClipRect.y - draw_data->DisplayPos.y) * draw_data->FramebufferScale.y),
                sign_cast<u32>((imgui_cmd.ClipRect.z - draw_data->DisplayPos.x) * draw_data->FramebufferScale.x),
                sign_cast<u32>((imgui_cmd.ClipRect.w - draw_data->DisplayPos.y) * draw_data->FramebufferScale.y)
            );
            scissor.width = scissor.width - sign_cast<u32>(scissor.x_offset);
            scissor.height = scissor.height - sign_cast<u32>(scissor.y_offset);

            cmd->set_scissor(cmd_buf, scissor);
            cmd->draw_indexed(
                cmd_buf,
                backend->shader->pipeline_desc,
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

void new_frame(ImGuiBackend* backend, const WindowHandle window_handle)
{
    ImGui::SetCurrentContext(backend->ctx);

    auto& io = ImGui::GetIO();
    // Set display size
    io.DisplaySize = g_platform->get_window_size(window_handle).to_float2();
    if (io.DisplaySize.x > 0 && io.DisplaySize.y > 0)
    {
        const auto framebuffer_size = g_platform->get_framebuffer_size(window_handle).to_float2();
        io.DisplayFramebufferScale = framebuffer_size / float2(io.DisplaySize);
    }
    // TODO(Jacob): dpi/monitor info
//    auto& platform_io = ImGui::GetPlatformIO();

    // Update DT
    const u64 now = time::now();
    io.DeltaTime = static_cast<float>(time::total_seconds(now - backend->time));
    backend->time = now;

    // Update input
    auto* mouse = g_input->default_device(InputDeviceType::mouse);
    io.MouseDown[0] = mouse->get_state(MouseButton::left)->values[0].flag;
    io.MouseDown[1] = mouse->get_state(MouseButton::right)->values[0].flag;
    io.MouseDown[2] = mouse->get_state(MouseButton::middle)->values[0].flag;
    io.MousePos = g_platform->get_cursor_position(window_handle).to_float2();

    ImGui::NewFrame();
}


} // namespace bee

static bee::ImGuiBackendModule g_backend{};

extern void bee_load_imgui(bee::PluginLoader* loader, const bee::PluginState state);

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    bee_load_imgui(loader, state);

    // Render module
    g_backend.create_backend = bee::create_backend;
    g_backend.destroy_backend = bee::destroy_backend;
    g_backend.draw = bee::draw;
    g_backend.new_frame = bee::new_frame;
    loader->set_module(BEE_IMGUI_BACKEND_MODULE_NAME, &g_backend, state);

    bee::g_shader_pipeline = static_cast<bee::ShaderPipelineModule*>(loader->get_module(BEE_SHADER_PIPELINE_MODULE_NAME));
    bee::g_platform = static_cast<bee::PlatformModule*>(loader->get_module(BEE_PLATFORM_MODULE_NAME));
    bee::g_asset_pipeline = static_cast<bee::AssetPipelineModule*>(loader->get_module(BEE_ASSET_PIPELINE_MODULE_NAME));
    bee::g_input = static_cast<bee::InputModule*>(loader->get_module(BEE_INPUT_MODULE_NAME));
}
