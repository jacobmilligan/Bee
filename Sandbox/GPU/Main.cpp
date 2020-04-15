/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include <Bee/Application/Application.hpp>
#include <Bee/Core/Bee.hpp>
#include <Bee/Core/Memory/MemoryTracker.hpp>
#include <Bee/Core/Time.hpp>
#include <Bee/Graphics/Command.hpp>
#include <Bee/Graphics/GPU.hpp>

namespace bee {


struct GPUApp
{
    DeviceHandle                    device;
    SwapchainHandle                 swapchain;
    RenderPassHandle                render_pass;
    PipelineStateHandle             pipeline;
    CommandBatcher                  cmd_context;
    CommandAllocator                cmd_allocator;
    CommandBuffer                   cmd;
    FixedArray<CommandBuffer*>   gpu_cmd;
    FenceHandle                     frame_fence;

    /*
     * Initialize a command allocator - in a production app you would have one of these per worker thread to
     * allow for multithreaded command generation
     */
    GPUApp()
        : cmd_allocator(kibibytes(1))
    {}
};


int on_launch(AppContext* ctx)
{
    auto app = static_cast<GPUApp*>(ctx->user_data);

    /*
     * Enumerate all the available physical device and query their capabilities
     */

    // if the buffer parameter is null or buffer size is zero then this will just return the number of available devices
    const auto physical_device_count = gpu_enumerate_physical_devices(nullptr, 0);
    auto physical_devices = FixedArray<PhysicalDeviceInfo>::with_size(physical_device_count, temp_allocator());
    gpu_enumerate_physical_devices(physical_devices.data(), physical_devices.size());

    for (auto& device : physical_devices)
    {
        log_info(
            "Device %d:\n  Name: %s\n  API: %s\n  Type: %s\n  Vendor: %s",
            device.id,
            device.name,
            device.api_version,
            gpu_type_string(device.type),
            gpu_vendor_string(device.vendor)
        );
    }

    /*
     * Create the logical GPU device
     */
    DeviceCreateInfo device_info{};
    device_info.physical_device_id = 0;

    app->device = gpu_create_device(device_info);

    /*
     * Initialize the command compiler - this handles compiling all command buffers for a frame, distributing the
     * work across multiple threads
     */
    new (&app->cmd_context) CommandBatcher(app->device);

    /*
     * Create a swapchain to present to
     */
    SwapchainCreateInfo swapchain_info{};
    swapchain_info.texture_format = PixelFormat::bgra8;
    swapchain_info.texture_extent = Extent::from_platform_size(get_window_framebuffer_size(ctx->main_window));
    swapchain_info.texture_array_layers = 1;
    swapchain_info.vsync = true;
    swapchain_info.window = ctx->main_window;
    swapchain_info.debug_name = "Default swapchain";

    app->swapchain = gpu_create_swapchain(app->device, swapchain_info);

    /*
     * Create a render pass
     */
    SubPassDescriptor subpass{};
    subpass.color_attachments[0] = 0;

    RenderPassCreateInfo rp_info{};
    rp_info.attachment_count = 1;
    rp_info.attachments[0].type = AttachmentType::present;
    rp_info.attachments[0].format = swapchain_info.texture_format;
    rp_info.attachments[0].load_op = LoadOp::clear;
    rp_info.attachments[0].store_op = StoreOp::store;
    rp_info.subpass_count = 1;
    rp_info.subpasses = &subpass;

    app->render_pass = gpu_create_render_pass(app->device, rp_info);

    /*
     * Create a pipeline state
     */
//    PipelineStateCreateInfo ps_info{};
//    ps_info.compatible_render_pass = app->render_pass;
//    ps_info.vertex_description

    /*
     * Setup a command buffer - this will get reset every frame
     */
    new (&app->cmd) CommandBuffer(&app->cmd_allocator);

    return EXIT_SUCCESS;
}

void on_shutdown(AppContext* ctx)
{
    auto app = static_cast<GPUApp*>(ctx->user_data);

    destruct(&app->cmd_context);
    gpu_destroy_render_pass(app->device, app->render_pass);
    gpu_destroy_swapchain(app->device, app->swapchain);
    gpu_destroy_device(app->device);
}

void on_frame(AppContext* ctx)
{
    auto app = static_cast<GPUApp*>(ctx->user_data);

    poll_input(&ctx->default_input);

    if (key_typed(ctx->default_input, Key::escape))
    {
        ctx->quit = true;
        return;
    }

    auto& cmd = app->cmd;

    if (cmd.count() > 0)
    {
        cmd.reset();
    }

    gpu_acquire_swapchain_texture(app->device, app->swapchain);

    const auto swapchain_extent = gpu_get_swapchain_extent(app->device, app->swapchain);
    const auto swapchain_view = gpu_get_swapchain_texture_view(app->device, app->swapchain);
    const ClearValue clear(1.0f, 0.0f, 0.0f, 1.0);

    cmd.begin_render_pass(
        app->render_pass,
        1, &swapchain_view,
        RenderRect(0, 0, swapchain_extent.width, swapchain_extent.height),
        1, &clear
    );

    cmd.end_render_pass();

    if (app->frame_fence.is_valid())
    {
        gpu_wait_for_fence(app->device, app->frame_fence);
    }

    app->frame_fence = app->cmd_context.submit_batch(1, &cmd);

    gpu_present(app->device, app->swapchain);
    gpu_commit_frame(app->device);
}


} // namespace bee


int bee_main(int argc, char** argv)
{
    bee::GPUApp gpu_ctx{};
    bee::AppDescriptor app{};
    app.app_name = "Sandbox.GPU";
    app.on_launch = bee::on_launch;
    app.on_shutdown = bee::on_shutdown;
    app.on_frame = bee::on_frame;
    app.user_data = &gpu_ctx;

    return bee::app_run(app);
}

