/*
 *  AppLoop.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Application/AppLoop.hpp"
#include "Bee/Graphics/GPU.hpp"

namespace bee {


int app_run(const AppDescriptor& desc)
{
    // Initialize core systems before launching the platform
    job_system_init(desc.job_system_info);

    if (!platform_launch(desc.app_name))
    {
        return EXIT_FAILURE;
    }

    AppContext ctx{};
    ctx.user_data = desc.user_data;

    // Initialize platform
    input_buffer_init(&ctx.default_input);

    // Initialize graphics systems
    if (!gpu_init())
    {
        log_error("Failed to initialize GPU backend");
        return EXIT_FAILURE;
    }

    // Create main window
    ctx.main_window = create_window(desc.main_window_config);
    BEE_ASSERT(ctx.main_window.is_valid());

    // Launch the user app
    const int result = desc.on_launch(&ctx);
    if (result != EXIT_SUCCESS)
    {
        desc.on_shutdown(&ctx);
        return result;
    }

    /*
     * Main loop
     */
    while (!ctx.quit)
    {
        temp_allocator_reset();
        desc.on_frame(&ctx);
    }

    // shutdown the user app first
    desc.on_shutdown(&ctx);

    // Destroy graphics systems
    gpu_destroy();

    if (platform_is_running())
    {
        platform_shutdown(); // closes all windows by default
    }

    // Shutdown core systems last
    job_system_shutdown();

    return EXIT_SUCCESS;
}


} // namespace bee