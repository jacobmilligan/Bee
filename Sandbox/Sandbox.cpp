/*
 *  Sandbox.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Sandbox.hpp"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Time.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"

#include "Bee/Input/Input.hpp"
#include "Bee/Input/Keyboard.hpp"
#include "Bee/Input/Mouse.hpp"

#include "Bee/Gpu/Gpu.hpp"

#include "Bee/RenderGraph/RenderGraph.hpp"

#include "Bee/ShaderPipeline/Compiler.hpp"

#include "Bee/AssetPipeline/AssetPipeline.hpp"

#include "Bee/AssetCache/AssetCache.hpp"


namespace bee {


struct SandboxApp
{
    bool                                platform_running { false };
    bool                                needs_reload { false };
    GpuBackend*                         gpu {nullptr };
    RenderGraph*                        render_graph {nullptr };
    RenderGraphPass*                    render_graph_pass {nullptr };
    ShaderCache*                        shader_cache {nullptr };
    WindowHandle                        window;
    const InputDevice*                  keyboard { nullptr };
    const InputDevice*                  mouse { nullptr };
    DeviceHandle                        device;
    SwapchainHandle                     swapchain;

    // Asset loading
    AssetPipeline*                      asset_pipeline {nullptr };
    AssetCache*                         asset_cache { nullptr };
};

struct SandboxPassData
{
    RenderGraphResource backbuffer;
    int                 color;
    float               time;
    ClearValue          colors[3];
};

static PlatformModule*          g_platform { nullptr };
static InputModule*             g_input { nullptr };
static GpuModule*               g_gpu { nullptr };
static RenderGraphModule*       g_render_graph { nullptr };
static ShaderCompilerModule*    g_shader_compiler { nullptr };
static ShaderCacheModule*       g_shader_cache { nullptr };
static AssetDatabaseModule*     g_asset_db { nullptr };
static AssetPipelineModule*     g_asset_pipeline { nullptr };
static AssetCacheModule*        g_asset_cache { nullptr };

static SandboxApp*              g_app = nullptr;

// RenderGraph passes have three phases:
// - init: called only ONCE when the pass is registered to the graph - use this for creating persistent
//      resources used between frames or creating other data
// - setup: called serially at the beginning of each frame and used to specify the passes input/output dependencies.
// - execute: called in a job thread asynchronously if the pass wasn't culled by the graph - handles command buffer
//   generation and other GPU functions
static void init_pass(GpuBackend* gpu, const DeviceHandle device, const void* external_data, void* pass_data)
{
    auto* sandbox_pass = static_cast<SandboxPassData*>(pass_data);

    sandbox_pass->colors[0] = ClearValue(1.0f, 0.2f, 0.3f, 1.0f);
    sandbox_pass->colors[1] = ClearValue(0.0f, 1.0f, 0.2f, 1.0f);
    sandbox_pass->colors[2] = ClearValue(0.1f, 0.3f, 1.0f, 1.0f);
}

static void setup_pass(RenderGraphPass* pass, RenderGraphBuilderModule* builder, const void* external_data, void* pass_data)
{
    auto* sandbox_pass = static_cast<SandboxPassData*>(pass_data);

    sandbox_pass->time += 0.01f;
    if (sandbox_pass->time > 1.0f)
    {
        sandbox_pass->time = 0.0f;
        sandbox_pass->color = (sandbox_pass->color + 1) % static_array_length(sandbox_pass->colors);
    }

    // specify that we're 'importing' an external resource (the swapchain backbuffer) and that it has a
    // dependency on this pass by specifying an attachment write
    sandbox_pass->backbuffer = builder->import_backbuffer(pass, "Swapchain", g_app->swapchain);
    builder->write_color(pass, sandbox_pass->backbuffer, LoadOp::clear, StoreOp::store, 1);
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
    auto* sandbox_pass = static_cast<SandboxPassData*>(pass_data);

    // Get the concrete GPU resources from the virtual RenderGraphPass object
    const TextureViewHandle* attachments = nullptr;
    const int attachment_count = storage->get_attachments(pass, &attachments);
    const auto backbuffer_rect = storage->get_backbuffer_rect(pass, sandbox_pass->backbuffer);

    // advance to the next colour to show a colour spectrum effect via clear colours and a render pass
    auto& current_color = sandbox_pass->colors[sandbox_pass->color].color;
    auto& next_color = sandbox_pass->colors[(sandbox_pass->color + 1) % static_array_length(sandbox_pass->colors)].color;

    ClearValue clear_value(
        math::lerp(current_color[0], next_color[0], sandbox_pass->time),
        math::lerp(current_color[1], next_color[1], sandbox_pass->time),
        math::lerp(current_color[2], next_color[2], sandbox_pass->time),
        1.0f
    );

    // All draw calls etc. must take place within a render pass and because we're using the render graph we can
    // just use the automatically-created one for this pass
    cmd->begin_render_pass(
        cmdbuf,
        storage->get_gpu_pass(pass),
        attachment_count, attachments,
        backbuffer_rect,
        attachment_count, &clear_value
    );
    // scissor and viewport are dynamic states by default so need to be set each frame
    cmd->set_scissor(cmdbuf, backbuffer_rect);
    cmd->set_viewport(cmdbuf, Viewport(0, 0, static_cast<float>(backbuffer_rect.width), static_cast<float>(backbuffer_rect.height)));
    cmd->end_render_pass(cmdbuf);
}

static bool startup()
{
    // Initialize the OS + app exe and register input default input devices
    if (!g_platform->start("Bee.Sandbox"))
    {
        return false;
    }

    g_app->platform_running = true;

    // Create the main app window
    WindowCreateInfo window_info{};
    window_info.title = "Bee Sandbox";
    window_info.monitor = g_platform->get_primary_monitor()->handle;
    g_app->window = g_platform->create_window(window_info);
    if (!g_app->window.is_valid())
    {
        return false;
    }

    // Get the default keyboard and mouse input devices
    g_app->keyboard = g_input->default_device(InputDeviceType::keyboard);
    g_app->mouse = g_input->default_device(InputDeviceType::mouse);

    if (g_app->keyboard == nullptr || g_app->mouse == nullptr)
    {
        return false;
    }

    log_info("Keyboard: %s", g_app->keyboard->name);
    log_info("Mouse: %s", g_app->mouse->name);

    // Initialize Vulkan backend and device
    g_app->gpu = g_gpu->get_default_backend(GpuApi::vulkan);

    if (g_app->gpu == nullptr || !g_app->gpu->init())
    {
        log_error("Failed to load Vulkan backend");
        return false;
    }

    g_app->device = g_app->gpu->create_device(DeviceCreateInfo{0 });
    if (!g_app->device.is_valid())
    {
        log_error("Failed to create Vulkan device");
        return false;
    }

    const auto fb_size = g_platform->get_framebuffer_size(g_app->window);

    // create a new swapchain for presenting the final backbuffer
    SwapchainCreateInfo swapchain_info{};
    swapchain_info.vsync = true;
    swapchain_info.window = g_app->window;
    swapchain_info.debug_name = "SandboxSwapchain";
    swapchain_info.texture_format = PixelFormat::rgba8;
    swapchain_info.texture_extent.width = sign_cast<u32>(fb_size.x);
    swapchain_info.texture_extent.height = sign_cast<u32>(fb_size.y);
    g_app->swapchain = g_app->gpu->create_swapchain(g_app->device, swapchain_info);

    if (!g_app->swapchain.is_valid())
    {
        log_error("Failed to create swapchain");
        return false;
    }

    // Create a new render graph to process the frame - manages creating GPU resources, automatic barriers etc.
    g_app->render_graph = g_render_graph->create_graph(g_app->gpu, g_app->device);

    // Now that we have a successful gpu backend - initialize the shader pipeline so we can compile the .bsc format
    // shader files that describe a whole pipeline state into GPU shader variants
    if (!g_shader_compiler->init())
    {
        return false;
    }

    // Create a new shader cache to hold different shader variants - shaders can bee found by name as well as hash
    g_app->shader_cache = g_shader_cache->create();
    if (g_app->shader_cache == nullptr)
    {
        return false;
    }

    // Setup the asset pipeline - this manages both the asset database (mapping GUID->metadata and artifact buffers)
    // and registered importers for the various asset types.
    const auto pipeline_path = bee::fs::roots().installation.join("Sandbox/AssetPipeline.json", temp_allocator());
    g_app->asset_pipeline = g_asset_pipeline->load_pipeline(pipeline_path.view());
    if (g_app->asset_pipeline == nullptr)
    {
        return false;
    }

    // Register the shader compiler importer for importing .bsc files into our new asset pipeline
    g_shader_compiler->register_importer(g_app->asset_pipeline, g_app->shader_cache);

    // Create a new asset cache for storing assets loaded at runtime
    g_app->asset_cache = g_asset_cache->create_cache();
    if (g_app->asset_cache == nullptr)
    {
        return false;
    }
    // Set the runtime cache used by the asset pipeline to notify for asset hot-reload when reimporting asset files
    g_asset_pipeline->set_runtime_cache(g_app->asset_pipeline, g_app->asset_cache);

    // Register the shader asset loader for loading the ShaderPipeline objects produced by importing .bsc files
    g_shader_cache->register_asset_loader(g_app->shader_cache, g_app->asset_cache, g_app->gpu, g_app->device);

    return true;
}

static void shutdown()
{
    // Cleanup is the same as startup but in reverse-order
    if (g_app->asset_cache != nullptr)
    {
        if (g_app->shader_cache != nullptr)
        {
            g_shader_cache->unregister_asset_loader(g_app->shader_cache, g_app->asset_cache);
        }
        g_asset_pipeline->set_runtime_cache(g_app->asset_pipeline, nullptr);
        g_asset_cache->destroy_cache(g_app->asset_cache);
        g_app->asset_cache = nullptr;
    }

    if (g_app->shader_cache != nullptr)
    {
        g_shader_cache->destroy(g_app->shader_cache);
        g_app->shader_cache = nullptr;
        g_shader_compiler->destroy();
    }

    if (g_app->asset_pipeline != nullptr)
    {
        g_asset_pipeline->destroy_pipeline(g_app->asset_pipeline);
        g_app->asset_pipeline = nullptr;
    }

    if (g_app->gpu != nullptr && g_app->gpu->is_initialized())
    {
        if (g_render_graph != nullptr && g_app->render_graph != nullptr)
        {
            g_render_graph->destroy_graph(g_app->render_graph);
        }

        if (g_app->device.is_valid())
        {
            // the submissions will have already been flushed by destroying the render graph
            if (g_app->swapchain.is_valid())
            {
                g_app->gpu->destroy_swapchain(g_app->device, g_app->swapchain);
                g_app->swapchain = SwapchainHandle{};
            }

            g_app->gpu->destroy_device(g_app->device);
            g_app->device = DeviceHandle{};
        }

        g_app->gpu->destroy();
    }

    if (g_app->window.is_valid())
    {
        g_platform->destroy_window(g_app->window);
    }

    if (g_platform != nullptr && g_app->platform_running)
    {
        g_platform->shutdown();
        g_app->platform_running = false;
    }
}

static void reload_plugin()
{
    if (g_app->render_graph_pass != nullptr)
    {
        g_render_graph->remove_pass(g_app->render_graph_pass);
    }

    g_app->render_graph_pass = g_render_graph->add_pass<SandboxPassData>(
        g_app->render_graph,
        "SandboxPass",
        setup_pass,
        execute_pass,
        init_pass
    );
}

static bool tick()
{
    // Close the app if either the window is closed or the apps quit event fired
    if (g_platform->quit_requested() || g_platform->window_close_requested(g_app->window))
    {
        return false;
    }

    // Reset the global per-frame threadsafe temp allocator used by the runtime
    temp_allocator_reset();
    // Refresh the asset pipeline and process any directory events detected at the root paths
    g_asset_pipeline->refresh(g_app->asset_pipeline);

    // Reload the sandbox plugin if needed
    if (g_app->needs_reload)
    {
        reload_plugin();
        g_app->needs_reload = false;
    }

    // Poll input for the app and show some info logs for when the keyboard/mouse was triggered
    g_platform->poll_input();

    const bool escape_typed = g_app->keyboard->get_state(Key::escape)->values[0].flag
        && !g_app->keyboard->get_previous_state(Key::escape)->values[0].flag;
    if (escape_typed)
    {
        return false;
    }

    const bool left_mouse_clicked = g_app->mouse->get_state(MouseButton::left)->values[0].flag
        && !g_app->mouse->get_previous_state(MouseButton::left)->values[0].flag;
    if (left_mouse_clicked)
    {
        log_info("Clicked!");
    }

    // Setup and Execute the render graph and then commit the resulting frame to the GPU for present
    g_render_graph->setup(g_app->render_graph);
    g_render_graph->execute(g_app->render_graph);
    g_app->gpu->commit_frame(g_app->device);
    return true;
}


} // namespace bee

static bee::SandboxModule g_module;

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    // Plugin dependencies are specified by calling loader->require_plugin and passing a plugin name
    // and minimum required version. If the plugin isn't found or the version doesn't match, it will return false
    if (!loader->require_plugin("Bee.VulkanBackend", bee::PluginVersion{ 0, 0, 0 }))
    {
        bee::log_error("Missing dependency: Bee.VulkanBackend");
        return;
    }

    if (!loader->require_plugin("Bee.RenderGraph", bee::PluginVersion{ 0, 0, 0 }))
    {
        bee::log_error("Missing dependency: Bee.RenderGraph");
        return;
    }

    if (!loader->require_plugin("Bee.ShaderPipeline", bee::PluginVersion{0, 0, 0}))
    {
        bee::log_error("Missing dependency: Bee.ShaderPipeline");
        return;
    }

    if (!loader->require_plugin("Bee.AssetPipeline", bee::PluginVersion{0, 0, 0}))
    {
        bee::log_error("Missing dependency: Bee.AssetPipeline");
        return;
    }

    if (!loader->require_plugin("Bee.AssetCache", bee::PluginVersion{0, 0, 0}))
    {
        bee::log_error("Missing dependency: Bee.AssetCache");
        return;
    }

    // Create a new static variable that persists between plugin reloads for storing the app state
    bee::g_app = loader->get_static<bee::SandboxApp>("Bee.SandboxApp");

    // Register our new application module with the api function pointers assigned. set_module will automatically
    // add/remove the module based on the plugin loading `state` (loading/unloading)
    g_module.startup = bee::startup;
    g_module.shutdown = bee::shutdown;
    g_module.tick = bee::tick;
    loader->set_module(BEE_SANDBOX_MODULE_NAME, &g_module, state);

    // If the sandbox plugin is loading then we should grab all the module pointers we'll need for the app
    if (state == bee::PluginState::loading)
    {
        bee::g_app->needs_reload = true;
        bee::g_platform = static_cast<bee::PlatformModule*>(loader->get_module(BEE_PLATFORM_MODULE_NAME));
        bee::g_input = static_cast<bee::InputModule*>(loader->get_module(BEE_INPUT_MODULE_NAME));
        bee::g_render_graph = static_cast<bee::RenderGraphModule*>(loader->get_module(BEE_RENDER_GRAPH_MODULE_NAME));
        bee::g_gpu = static_cast<bee::GpuModule*>(loader->get_module(BEE_GPU_MODULE_NAME));
        bee::g_shader_compiler = static_cast<bee::ShaderCompilerModule*>(loader->get_module(BEE_SHADER_COMPILER_MODULE_NAME));
        bee::g_shader_cache = static_cast<bee::ShaderCacheModule*>(loader->get_module(BEE_SHADER_CACHE_MODULE_NAME));
        bee::g_asset_db = static_cast<bee::AssetDatabaseModule*>(loader->get_module(BEE_ASSET_DATABASE_MODULE_NAME));
        bee::g_asset_pipeline = static_cast<bee::AssetPipelineModule*>(loader->get_module(BEE_ASSET_PIPELINE_MODULE_NAME));
        bee::g_asset_cache = static_cast<bee::AssetCacheModule*>(loader->get_module(BEE_ASSET_CACHE_MODULE_NAME));
    }
}

BEE_PLUGIN_VERSION(0, 0, 0)