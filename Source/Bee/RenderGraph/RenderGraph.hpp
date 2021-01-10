/*
 *  RenderGraph.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Handle.hpp"

#include "Bee/Gpu/Gpu.hpp"


namespace bee {


struct RenderGraph;
struct RenderGraphPass;
BEE_SPLIT_HANDLE(RenderGraphResource, u64, 56, 8, index, type);


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
    return static_cast<u32>(type) == resource.type();
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

struct RenderGraphStorage
{
    BufferHandle (*get_buffer)(RenderGraphPass* pass, const RenderGraphResource& handle) { nullptr };

    TextureHandle (*get_texture)(RenderGraphPass* pass, const RenderGraphResource& handle) { nullptr };

    u32 (*get_attachments)(RenderGraphPass* pass, const TextureViewHandle** dst) { nullptr };

    RenderPassHandle (*get_gpu_pass)(RenderGraphPass* pass) { nullptr };

    Extent (*get_backbuffer_size)(RenderGraphPass* pass, const RenderGraphResource& handle) { nullptr };

    RenderRect (*get_backbuffer_rect)(RenderGraphPass* pass, const RenderGraphResource& handle) { nullptr };

    DeviceHandle (*get_device)(RenderGraphPass* pass) { nullptr };
};

#define BEE_RENDER_GRAPH_BUILDER_MODULE_NAME "BEE_RENDER_GRAPH_BUILDER"

struct RenderGraphBuilderModule;

using render_graph_setup_pass_t = void(*)(RenderGraphPass* pass, RenderGraphBuilderModule* builder, const void* external_data, void* pass_data);
using render_graph_execute_pass_t = void(*)(RenderGraphPass* pass, RenderGraphStorage* storage, GpuCommandBackend* cmd, CommandBuffer* cmdbuf, const void* external_data, void* pass_data);
using render_graph_init_pass_t = void(*)(GpuBackend* backend, const DeviceHandle device, const void* external_data, void* pass_data);


struct RenderGraphPassDesc
{
    const char*                 name { nullptr };
    const void*                 external_data { nullptr };
    size_t                      external_data_size { 0 };
    size_t                      pass_data_size { 0 };
    render_graph_init_pass_t    init { nullptr };
    render_graph_init_pass_t    destroy { nullptr };
    render_graph_setup_pass_t   setup { nullptr };
    render_graph_execute_pass_t execute { nullptr };
};

struct RenderGraphBuilderModule
{
    void (*disable_pass)(RenderGraphPass* pass) { nullptr };

    void (*import_render_pass)(RenderGraphPass* pass, const RenderPassHandle& handle, const i32 attachment_count, const AttachmentDescriptor* attachments, const RenderGraphResource* resources) { nullptr };

    RenderGraphResource (*create_buffer)(RenderGraphPass* pass, const char* name, const BufferCreateInfo& create_info) { nullptr };

    RenderGraphResource (*create_texture)(RenderGraphPass* pass, const char* name, const TextureCreateInfo& create_info) { nullptr };

    RenderGraphResource (*import_buffer)(RenderGraphPass* pass, const char* name, const BufferHandle& buffer) { nullptr };

    RenderGraphResource (*import_texture)(RenderGraphPass* pass, const char* name, const TextureHandle& texture) { nullptr };

    RenderGraphResource (*import_backbuffer)(RenderGraphPass* pass, const char* name, const SwapchainHandle& swapchain) { nullptr };

    void (*write_color)(RenderGraphPass* pass, const RenderGraphResource& texture, const LoadOp load, const StoreOp store, const u32 samples) { nullptr };

    void (*write_depth)(RenderGraphPass* pass, const RenderGraphResource& texture, const PixelFormat depth_format, const LoadOp load, const StoreOp store) { nullptr };
};

#define BEE_RENDER_GRAPH_MODULE_NAME "BEE_RENDER_GRAPH"

struct RenderGraphModule
{
    RenderGraph* (*create_graph)(GpuBackend* backend, const DeviceHandle device) { nullptr };

    void (*destroy_graph)(RenderGraph* graph) { nullptr };

    RenderGraphPass* (*add_static_pass)(RenderGraph* graph, const RenderGraphPassDesc& desc) { nullptr };

    void (*remove_pass)(RenderGraphPass* pass) { nullptr };

    void (*setup)(RenderGraph* graph) { nullptr };

    void (*execute)(RenderGraph* graph) { nullptr };

    template <typename PassDataType, typename ExternalDataType>
    RenderGraphPass* add_pass(
        RenderGraph* graph,
        const char* name,
        const ExternalDataType& external_data,
        render_graph_setup_pass_t setup_pass,
        render_graph_execute_pass_t execute_pass,
        render_graph_init_pass_t init_pass = nullptr,
        render_graph_init_pass_t destroy_pass = nullptr
    )
    {
        static_assert(std::is_trivially_copyable_v<ExternalDataType>, "ExternalDataType is not trivially copyable");
        static_assert(std::is_trivially_copyable_v<PassDataType>, "PassDataType is not trivially copyable");

        RenderGraphPassDesc desc{};
        desc.external_data_size = sizeof(ExternalDataType);
        desc.external_data = &external_data;
        desc.pass_data_size = sizeof(PassDataType);
        desc.init = init_pass;
        desc.destroy = destroy_pass;
        desc.setup = setup_pass;
        desc.execute = execute_pass;
        return add_static_pass(graph, desc);
    }

    template <typename PassDataType>
    RenderGraphPass* add_pass(
        RenderGraph* graph,
        const char* name,
        render_graph_setup_pass_t setup_pass,
        render_graph_execute_pass_t execute_pass,
        render_graph_init_pass_t init_pass = nullptr,
        render_graph_init_pass_t destroy_pass = nullptr
    )
    {
        static_assert(std::is_trivially_copyable_v<PassDataType>, "PassDataType is not trivially copyable");

        RenderGraphPassDesc desc{};
        desc.external_data_size = 0;
        desc.external_data = nullptr;
        desc.pass_data_size = sizeof(PassDataType);
        desc.init = init_pass;
        desc.destroy = destroy_pass;
        desc.setup = setup_pass;
        desc.execute = execute_pass;
        return add_static_pass(graph, desc);
    }
};


} // namespace bee