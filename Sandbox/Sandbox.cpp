/*
 *  Sandbox.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Main.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

#include "Bee/Input/Input.hpp"
#include "Bee/Input/Keyboard.hpp"
#include "Bee/Input/Mouse.hpp"

#include "Bee/Gpu/Gpu.hpp"

#include "Bee/RenderGraph/RenderGraph.hpp"

struct SandboxApp
{
    bool                    platform_running { false };
    bee::PlatformModule*    platform { nullptr };
    bee::InputModule*       input { nullptr };
    bee::GpuModule*         gpu { nullptr };
    bee::GpuBackend*        backend { nullptr };
    bee::RenderGraphModule* render_graph { nullptr };
    bee::RenderGraph*       graph { nullptr };
    bee::WindowHandle       window;
    const bee::InputDevice* keyboard { nullptr };
    const bee::InputDevice* mouse { nullptr };
    bee::DeviceHandle       device;
    bee::SwapchainHandle    swapchain;
};

struct SandboxPassData
{
    bee::RenderGraphResource backbuffer;
};

static void setup_pass(bee::RenderGraphPass* pass, bee::RenderGraphBuilderModule* builder, const void* external_data, void* pass_data)
{
    const auto* app = *static_cast<SandboxApp* const*>(external_data);
    auto* sandbox_pass = static_cast<SandboxPassData*>(pass_data);
    sandbox_pass->backbuffer = builder->import_backbuffer(pass, "Swapchain", app->swapchain);
    builder->write_color(pass, sandbox_pass->backbuffer, bee::LoadOp::dont_care, bee::StoreOp::store, 1);
}

static void execute_pass(
    bee::RenderGraphPass* pass,
    bee::RenderGraphStorage* storage,
    bee::GpuCommandBackend* cmd,
    bee::CommandBuffer* cmdbuf,
    const void* external_data,
    void* pass_data
)
{
    auto* sandbox_pass = static_cast<SandboxPassData*>(pass_data);
    const bee::TextureViewHandle* attachments = nullptr;
    const int attachment_count = storage->get_attachments(pass, &attachments);
    bee::ClearValue clear_value(0.0f, 0.0f, 0.0f, 0.0f);

    cmd->begin_render_pass(
        cmdbuf,
        storage->get_gpu_pass(pass),
        attachment_count, attachments,
        storage->get_backbuffer_rect(pass, sandbox_pass->backbuffer),
        attachment_count, &clear_value
    );
    cmd->end_render_pass(cmdbuf);
}

static bool start_app(SandboxApp* app)
{
    bee::init_plugins();
    bee::add_plugin_search_path(bee::fs::get_root_dirs().binaries_root.join("Plugins"));
    bee::refresh_plugins();
    // Vulkan has deps on GPU & Platform
    if (!bee::load_plugin("Bee.VulkanBackend"))
    {
        return false;
    }

    if (!bee::load_plugin("Bee.RenderGraph"))
    {
        return false;
    }

    app->platform = static_cast<bee::PlatformModule*>(bee::get_module(BEE_PLATFORM_MODULE_NAME));
    app->input = static_cast<bee::InputModule*>(bee::get_module(BEE_INPUT_MODULE_NAME));

    if (!app->platform->start("Bee.Sandbox"))
    {
        return false;
    }

    app->platform_running = true;

    bee::WindowCreateInfo window_info{};
    window_info.title = "Bee Sandbox";
    window_info.monitor = app->platform->get_primary_monitor()->handle;
    app->window = app->platform->create_window(window_info);
    if (!app->window.is_valid())
    {
        return false;
    }

    app->keyboard = app->input->default_device(bee::InputDeviceType::keyboard);
    app->mouse = app->input->default_device(bee::InputDeviceType::mouse);

    if (app->keyboard == nullptr || app->mouse == nullptr)
    {
        return false;
    }

    bee::log_info("Keyboard: %s", app->keyboard->name);
    bee::log_info("Mouse: %s", app->mouse->name);

    // Initialize Vulkan backend
    app->gpu = static_cast<bee::GpuModule*>(bee::get_module(BEE_GPU_MODULE_NAME));
    app->backend = app->gpu->get_default_backend(bee::GpuApi::vulkan);

    if (app->backend == nullptr || !app->backend->init())
    {
        bee::log_error("Failed to load Vulkan backend");
        return false;
    }

    app->device = app->backend->create_device(bee::DeviceCreateInfo{ 0 });
    if (!app->device.is_valid())
    {
        bee::log_error("Failed to create Vulkan device");
        return false;
    }

    const auto fb_size = app->platform->get_framebuffer_size(app->window);

    bee::SwapchainCreateInfo swapchain_info{};
    swapchain_info.vsync = false;
    swapchain_info.window = app->window;
    swapchain_info.debug_name = "SandboxSwapchain";
    swapchain_info.texture_format = bee::PixelFormat::rgba8;
    swapchain_info.texture_extent.width = bee::sign_cast<bee::u32>(fb_size.x);
    swapchain_info.texture_extent.height = bee::sign_cast<bee::u32>(fb_size.y);
    app->swapchain = app->backend->create_swapchain(app->device, swapchain_info);

    if (!app->swapchain.is_valid())
    {
        bee::log_error("Failed to create swapchain");
        return false;
    }

    app->render_graph = static_cast<bee::RenderGraphModule*>(bee::get_module(BEE_RENDER_GRAPH_MODULE));
    app->graph = app->render_graph->create_graph(app->backend, app->device);
    app->render_graph->add_pass<SandboxPassData>(app->graph, "SandboxPass", app, setup_pass, execute_pass);
    return true;
}

static void cleaup_app(SandboxApp* app)
{
    if (app->backend != nullptr && app->backend->is_initialized())
    {
        if (app->render_graph != nullptr && app->graph != nullptr)
        {
            app->render_graph->destroy_graph(app->graph);
        }

        if (app->device.is_valid())
        {
            // the submissions will have already been flushed by destroying the render graph
            if (app->swapchain.is_valid())
            {
                app->backend->destroy_swapchain(app->device, app->swapchain);
                app->swapchain = bee::SwapchainHandle{};
            }

            app->backend->destroy_device(app->device);
            app->device = bee::DeviceHandle{};
        }

        app->backend->destroy();
    }

    if (app->window.is_valid())
    {
        app->platform->destroy_window(app->window);
    }

    if (app->platform != nullptr && app->platform_running)
    {
        app->platform->shutdown();
        app->platform_running = false;
    }
}

static bool tick_app(SandboxApp* app)
{
    if (app->platform->quit_requested() || app->platform->window_close_requested(app->window))
    {
        return false;
    }

    app->platform->poll_input();

    const bool escape_typed = app->keyboard->get_state(bee::Key::escape)->values[0].flag
                           && !app->keyboard->get_previous_state(bee::Key::escape)->values[0].flag;
    if (escape_typed)
    {
        return false;
    }

    const bool left_mouse_clicked = app->mouse->get_state(bee::MouseButton::left)->values[0].flag
                                 && !app->mouse->get_previous_state(bee::MouseButton::left)->values[0].flag;
    if (left_mouse_clicked)
    {
        bee::log_info("Clicked!");
    }

    app->render_graph->setup(app->graph);
    app->render_graph->execute(app->graph);
    app->backend->commit_frame(app->device);
    return true;
}

int bee_main(int argc, char** argv)
{
    bee::JobSystemInitInfo job_system_info{};
    bee::job_system_init(job_system_info);

    SandboxApp app{};

    if (!start_app(&app))
    {
        cleaup_app(&app);
        return EXIT_FAILURE;
    }

    while (true)
    {
        bee::refresh_plugins();
        if (!tick_app(&app))
        {
            break;
        }
    }

    cleaup_app(&app);

    bee::job_system_shutdown();
    return EXIT_SUCCESS;
}