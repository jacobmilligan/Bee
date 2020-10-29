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

struct SandboxApp
{
    bool                    platform_running { false };
    bee::PlatformModule*    platform { nullptr };
    bee::InputModule*       input { nullptr };
    bee::GpuModule*         gpu { nullptr };
    bee::GpuBackend*        backend { nullptr };
    bee::WindowHandle       window;
    const bee::InputDevice* keyboard { nullptr };
    const bee::InputDevice* mouse { nullptr };
    bee::DeviceHandle       device;
    bee::SwapchainHandle    swapchain;
    bee::RenderPassHandle   render_pass;
    bee::GpuCommandBuffer   cmd;
};

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
    swapchain_info.vsync = true;
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

    bee::RenderPassCreateInfo rp_info{};
    bee::SubPassDescriptor subpass{};
    subpass.color_attachment_count = 1;
    subpass.color_attachments[0] = 0;
    rp_info.subpass_count = 1;
    rp_info.subpasses = &subpass;
    rp_info.attachment_count = 1;
    rp_info.attachments[0].type = bee::AttachmentType::present;
    rp_info.attachments[0].format = bee::PixelFormat::rgba8;
    rp_info.attachments[0].store_op = bee::StoreOp::store;
    app->render_pass = app->backend->create_render_pass(app->device, rp_info);

    if (!app->render_pass.is_valid())
    {
        bee::log_error("Failed to create render pass");
        return false;
    }

    return true;
}

static void cleaup_app(SandboxApp* app)
{
    if (app->backend != nullptr && app->backend->is_initialized())
    {
        if (app->device.is_valid())
        {
            app->backend->submissions_wait(app->device);

            if (app->render_pass.is_valid())
            {
                app->backend->destroy_render_pass(app->device, app->render_pass);
                app->render_pass = bee::RenderPassHandle{};
            }

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

    if (!app->backend->allocate_command_buffer(app->device, &app->cmd, bee::QueueType::graphics))
    {
        return true;
    }

    auto& cmd = app->cmd;
    cmd.begin(cmd.instance, bee::CommandBufferUsage::submit_once);
    {
        const auto fb_size = app->platform->get_framebuffer_size(app->window);
        const auto swapchain_texture = app->backend->get_swapchain_texture_view(app->device, app->swapchain);
        bee::ClearValue clear_value(0.0f, 0.0f, 0.0f, 0.0f);

        cmd.begin_render_pass(cmd.instance,
            app->render_pass,
            1, &swapchain_texture,
            bee::RenderRect(0, 0, static_cast<bee::u32>(fb_size.x), static_cast<bee::u32>(fb_size.y)),
            1, &clear_value);

        cmd.end_render_pass(cmd.instance);
    }
    cmd.end(cmd.instance);

    bee::SubmitInfo submit_info{};
    submit_info.command_buffer_count = 1;
    submit_info.command_buffers = &cmd.instance;
    app->backend->submit(app->device, submit_info);
    app->backend->present(app->device, app->swapchain);
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