/*
 *  Renderer.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Graphics/GPU.hpp"

namespace bee {


#define BEE_RENDER_GRAPH_BUILDER_MODULE_NAME "BEE_RENDER_GRAPH_BUILDER"

BEE_SPLIT_HANDLE(RenderGraphResource, i64, 56, 8);

struct RenderGraph;
struct RenderGraphPass;

struct RenderGraphStorage
{
    BufferHandle (*get_buffer)(RenderGraph* graph, const RenderGraphResource& handle) { nullptr };

    TextureHandle (*get_texture)(RenderGraph* graph, const RenderGraphResource& handle) { nullptr };
};

using render_graph_execute_t = void(*)(RenderGraph* graph, RenderGraphStorage* storage);

struct RenderGraphBuilderModule
{
    RenderGraph* (*create_render_graph)(const DeviceHandle device) { nullptr };

    void (*destroy_render_graph)(RenderGraph* graph) { nullptr };

    RenderGraphResource (*create_buffer)(RenderGraphPass* pass, const char* name, const BufferCreateInfo& create_info) { nullptr };

    RenderGraphResource (*create_texture)(RenderGraphPass* pass, const char* name, const TextureCreateInfo& create_info) { nullptr };

    void (*write_color)(RenderGraphPass* pass, const RenderGraphResource& texture, const LoadOp load, const StoreOp store) { nullptr };

    void (*write_depth)(RenderGraphPass* pass, const RenderGraphResource& texture, const PixelFormat depth_format, const LoadOp load, const StoreOp store) { nullptr };

    void (*set_execute_function)(RenderGraphPass* pass, render_graph_execute_t fn) { nullptr };

    RenderGraphPass* (*add_dynamic_pass)(RenderGraph* graph, const char* name, void** args, const size_t args_size) { nullptr };

    template <typename ArgsType>
    RenderGraphPass* add_pass(RenderGraph* graph, const char* name, ArgsType** args)
    {
        return add_dynamic_pass(graph, name, args, sizeof(ArgsType));
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

    void (*execute)(RenderGraphBuilderModule* builder, RenderGraph* graph) { nullptr };
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