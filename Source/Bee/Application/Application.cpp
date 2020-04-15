/*
 *  AppLoop.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Application/Application.hpp"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Graphics/GPU.hpp"

namespace bee {


void app_frame(const AppDescriptor& desc, AppContext* ctx)
{
    temp_allocator_reset();

    desc.on_frame(ctx);

    refresh_plugins();
}

int app_run(const AppDescriptor& desc)
{
    // Ensure plugin registry is initialized before anything else
    init_plugin_registry();

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
        // do a failure callback if launch was not successful instead of a normal app shutdown
        log_error("Failed to launch %s. Shutting down application.", desc.app_name);
        desc.on_fail(&ctx);
    }
    else
    {
        /*
        * Main loop
        */
        while (!ctx.quit)
        {
            app_frame(desc, &ctx);
        }

        // shutdown the user app first
        desc.on_shutdown(&ctx);
    }

    /*
     * shutdown plugin registry before core systems to ensure that all core systems are available
     * if a plugin has to use one in its unload function
     */
    destroy_plugin_registry();

    // Destroy graphics systems
    gpu_destroy();

    if (platform_is_running())
    {
        platform_shutdown(); // closes all windows by default
    }

    // Shutdown core systems last
    job_system_shutdown();

    return result;
}


} // namespace bee