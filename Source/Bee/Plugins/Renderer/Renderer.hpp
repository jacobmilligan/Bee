/*
 *  Renderer.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Graphics/GPU.hpp"
#include "Bee/Core/Functional.hpp"

namespace bee {


BEE_SPLIT_HANDLE(RenderGraphResource, u64, 56, 8);

enum class RenderGraphResourceType : u32
{
    buffer,
    texture,
    imported_buffer,
    imported_texture,
    backbuffer
};

constexpr bool operator==(const RenderGraphResourceType type, const RenderGraphResource& resource)
{
    return underlying_t(type) == resource.high();
}

constexpr bool operator!=(const RenderGraphResourceType type, const RenderGraphResource& resource)
{
    return !(type == resource);
}

constexpr bool operator==(const RenderGraphResource& resource, const RenderGraphResourceType type)
{
    return type == resource;
}

constexpr bool operator!=(const RenderGraphResource& resource, const RenderGraphResourceType type)
{
    return type != resource;
}


struct RenderGraph;
struct RenderGraphPass;
class JobGroup;

struct RenderGraphStorage
{
    BufferHandle (*get_buffer)(RenderGraphPass* pass, const RenderGraphResource& handle) { nullptr };

    TextureHandle (*get_texture)(RenderGraphPass* pass, const RenderGraphResource& handle) { nullptr };

    void (*begin_render_pass)(CommandBuffer* cmd, RenderGraphPass* pass, const RenderRect& render_area, const u32 clear_value_count, const ClearValue* clear_values) { nullptr };

    CommandBuffer* (*create_command_buffer)(RenderGraphPass* pass, const QueueType queue) { nullptr };

    Extent (*get_backbuffer_size)(RenderGraphPass* pass, const RenderGraphResource& handle) { nullptr };
};


#define BEE_RENDER_GRAPH_BUILDER_MODULE_NAME "BEE_RENDER_GRAPH_BUILDER"

using render_graph_execute_t = Function<void(RenderGraphPass* pass, RenderGraphStorage*), 1024>;

struct RenderGraphBuilderModule
{
    RenderGraph* (*create_render_graph)(Allocator* allocator) { nullptr };

    void (*destroy_render_graph)(RenderGraph* graph) { nullptr };

    void (*execute)(RenderGraph* graph, JobGroup* wait_handle) { nullptr };

    RenderGraphResource (*create_buffer)(RenderGraphPass* pass, const char* name, const BufferCreateInfo& create_info) { nullptr };

    RenderGraphResource (*create_texture)(RenderGraphPass* pass, const char* name, const TextureCreateInfo& create_info) { nullptr };

    RenderGraphResource (*import_buffer)(RenderGraphPass* pass, const char* name, const BufferHandle& buffer) { nullptr };

    RenderGraphResource (*import_texture)(RenderGraphPass* pass, const char* name, const TextureHandle& texture) { nullptr };

    RenderGraphResource (*import_backbuffer)(RenderGraphPass* pass, const char* name, const SwapchainHandle& swapchain) { nullptr };

    void (*write_color)(RenderGraphPass* pass, const RenderGraphResource& texture, const LoadOp load, const StoreOp store, const u32 samples) { nullptr };

    void (*write_depth)(RenderGraphPass* pass, const RenderGraphResource& texture, const PixelFormat depth_format, const LoadOp load, const StoreOp store) { nullptr };

    render_graph_execute_t& (*get_execute_function)(RenderGraphPass* pass) { nullptr };

    RenderGraphPass* (*add_pass)(RenderGraph* graph, const char* name) { nullptr };

    template <typename ExecuteFnType>
    void set_execute(RenderGraphPass* pass, ExecuteFnType&& execute_fn)
    {
        get_execute_function(pass) = execute_fn;
    }

    // Wrappers for RenderModule functions
    DeviceHandle (*get_device)() { nullptr };

    i32 (*get_swapchains)(SwapchainHandle* dst) { nullptr };

    SwapchainHandle (*get_primary_swapchain)() { nullptr };
};

struct RenderStage
{
    const char* (*get_name)() { nullptr };

    void (*init)(const DeviceHandle& device) { nullptr };

    void (*destroy)(const DeviceHandle& device) { nullptr };

    void (*execute)(RenderGraph* graph, RenderGraphBuilderModule* builder) { nullptr };
};


#define BEE_RENDERER_MODULE_NAME "BEE_RENDERER_MODULE"

enum class SwapchainKind
{
    /// creates a new swapchain and replaces the current primary swapchain
    primary     = 0,
    /// creates and appends a new secondary swapchain
    secondary   = 1
};

struct RendererModule
{
    bool (*init)(const DeviceCreateInfo& device_info) { nullptr };

    void (*destroy)() { nullptr };

    void (*execute_frame)() { nullptr };

    void (*add_stage)(RenderStage* stage) { nullptr };

    void (*remove_stage)(RenderStage* stage) { nullptr };

    DeviceHandle (*get_device)() { nullptr };

    void (*add_swapchain)(const SwapchainKind kind, const WindowHandle& window, const PixelFormat format, const char* name) { nullptr };

    void (*remove_swapchain)(const char* name) { nullptr };
};


} // namespace bee