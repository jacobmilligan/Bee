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

#include "Bee/Input/Input.hpp"
#include "Bee/Input/Keyboard.hpp"
#include "Bee/Input/Mouse.hpp"

#include "Bee/Gpu/Gpu.hpp"

#include "Bee/RenderGraph/RenderGraph.hpp"


namespace bee {


struct SandboxApp
{
    bool                platform_running { false };
    bool                needs_reload { false };
    GpuBackend*         backend { nullptr };
    RenderGraph*        graph { nullptr };
    RenderGraphPass*    pass { nullptr };
    WindowHandle        window;
    const InputDevice*  keyboard { nullptr };
    const InputDevice*  mouse { nullptr };
    DeviceHandle        device;
    SwapchainHandle     swapchain;
};

struct SandboxPassData
{
    RenderGraphResource backbuffer;
    int                 color;
    float               time;
    ClearValue          colors[3];
};

static PlatformModule*      g_platform { nullptr };
static InputModule*         g_input { nullptr };
static GpuModule*           g_gpu { nullptr };
static RenderGraphModule*   g_render_graph { nullptr };
static SandboxApp*          g_app = nullptr;


static void init_pass(const void* external_data, void* pass_data)
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
    const TextureViewHandle* attachments = nullptr;
    const int attachment_count = storage->get_attachments(pass, &attachments);
    const auto backbuffer_rect = storage->get_backbuffer_rect(pass, sandbox_pass->backbuffer);
    auto& current_color = sandbox_pass->colors[sandbox_pass->color].color;
    auto& next_color = sandbox_pass->colors[(sandbox_pass->color + 1) % static_array_length(sandbox_pass->colors)].color;

    ClearValue clear_value(
        math::lerp(current_color[0], next_color[0], sandbox_pass->time),
        math::lerp(current_color[1], next_color[1], sandbox_pass->time),
        math::lerp(current_color[2], next_color[2], sandbox_pass->time),
        1.0f
    );

    cmd->begin_render_pass(
        cmdbuf,
        storage->get_gpu_pass(pass),
        attachment_count, attachments,
        backbuffer_rect,
        attachment_count, &clear_value
    );
    cmd->set_scissor(cmdbuf, backbuffer_rect);
    cmd->set_viewport(cmdbuf, Viewport(0, 0, static_cast<float>(backbuffer_rect.width), static_cast<float>(backbuffer_rect.height)));
    cmd->end_render_pass(cmdbuf);
}

static bool startup()
{
    if (!g_platform->start("Bee.Sandbox"))
    {
        return false;
    }

    g_app->platform_running = true;

    WindowCreateInfo window_info{};
    window_info.title = "Bee Sandbox";
    window_info.monitor = g_platform->get_primary_monitor()->handle;
    g_app->window = g_platform->create_window(window_info);
    if (!g_app->window.is_valid())
    {
        return false;
    }

    g_app->keyboard = g_input->default_device(InputDeviceType::keyboard);
    g_app->mouse = g_input->default_device(InputDeviceType::mouse);

    if (g_app->keyboard == nullptr || g_app->mouse == nullptr)
    {
        return false;
    }

    log_info("Keyboard: %s", g_app->keyboard->name);
    log_info("Mouse: %s", g_app->mouse->name);

    // Initialize Vulkan backend
    g_app->backend = g_gpu->get_default_backend(GpuApi::vulkan);

    if (g_app->backend == nullptr || !g_app->backend->init())
    {
        log_error("Failed to load Vulkan backend");
        return false;
    }

    g_app->device = g_app->backend->create_device(DeviceCreateInfo{ 0 });
    if (!g_app->device.is_valid())
    {
        log_error("Failed to create Vulkan device");
        return false;
    }

    const auto fb_size = g_platform->get_framebuffer_size(g_app->window);

    SwapchainCreateInfo swapchain_info{};
    swapchain_info.vsync = true;
    swapchain_info.window = g_app->window;
    swapchain_info.debug_name = "SandboxSwapchain";
    swapchain_info.texture_format = PixelFormat::rgba8;
    swapchain_info.texture_extent.width = sign_cast<u32>(fb_size.x);
    swapchain_info.texture_extent.height = sign_cast<u32>(fb_size.y);
    g_app->swapchain = g_app->backend->create_swapchain(g_app->device, swapchain_info);

    if (!g_app->swapchain.is_valid())
    {
        log_error("Failed to create swapchain3");
        return false;
    }

    g_app->graph = g_render_graph->create_graph(g_app->backend, g_app->device);
    return true;
}

static void shutdown()
{
    if (g_app->backend != nullptr && g_app->backend->is_initialized())
    {
        if (g_render_graph != nullptr && g_app->graph != nullptr)
        {
            g_render_graph->destroy_graph(g_app->graph);
        }

        if (g_app->device.is_valid())
        {
            // the submissions will have already been flushed by destroying the render graph
            if (g_app->swapchain.is_valid())
            {
                g_app->backend->destroy_swapchain(g_app->device, g_app->swapchain);
                g_app->swapchain = SwapchainHandle{};
            }

            g_app->backend->destroy_device(g_app->device);
            g_app->device = DeviceHandle{};
        }

        g_app->backend->destroy();
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

static bool tick()
{
    if (g_platform->quit_requested() || g_platform->window_close_requested(g_app->window))
    {
        return false;
    }

    if (g_app->needs_reload)
    {
        if (g_app->pass != nullptr)
        {
            g_render_graph->remove_pass(g_app->pass);
        }

        g_app->pass = g_render_graph->add_pass<SandboxPassData>(
            g_app->graph,
            "SandboxPass",
            setup_pass,
            execute_pass,
            init_pass
        );
        g_app->needs_reload = false;
    }

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

    g_render_graph->setup(g_app->graph);
    g_render_graph->execute(g_app->graph);
    g_app->backend->commit_frame(g_app->device);
    return true;
}


} // namespace bee

static bee::SandboxModule g_module;

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
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

    bee::g_app = loader->get_static<bee::SandboxApp>("Bee.SandboxApp");

    g_module.startup = bee::startup;
    g_module.shutdown = bee::shutdown;
    g_module.tick = bee::tick;

    loader->set_module(BEE_SANDBOX_MODULE_NAME, &g_module, state);

    if (state == bee::PluginState::loading)
    {
        bee::g_app->needs_reload = true;
        bee::g_platform = static_cast<bee::PlatformModule*>(loader->get_module(BEE_PLATFORM_MODULE_NAME));
        bee::g_input = static_cast<bee::InputModule*>(loader->get_module(BEE_INPUT_MODULE_NAME));
        bee::g_render_graph = static_cast<bee::RenderGraphModule*>(loader->get_module(BEE_RENDER_GRAPH_MODULE));
        bee::g_gpu = static_cast<bee::GpuModule*>(loader->get_module(BEE_GPU_MODULE_NAME));
    }
}

BEE_PLUGIN_VERSION(0, 0, 0)