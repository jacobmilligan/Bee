/*
 *  AppLoop.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Application/AppLoop.hpp"
#include "Bee/Graphics/GPU.hpp"

namespace bee {


int app_loop(const AppLaunchConfig& config, Application* app)
{

    /*
     ************************************
     *
     * Engine initialization order:
     *  1. platform launch
     *  2. ctx alloc
     *  3. input buffer init
     *  4. gpu init
     *  5. main window create
     *
     ************************************
     */
    if (!platform_launch(config.app_name))
    {
        return EXIT_FAILURE;
    }

    AppContext ctx;

    // Initialize platform
    input_buffer_init(&ctx.default_input);

    // Initialize graphics systems
    if (!gpu_init())
    {
        log_error("Failed to initialize GPU backend");
        return EXIT_FAILURE;
    }

    // Create main window
    ctx.main_window = create_window(config.main_window_config);
    BEE_ASSERT(ctx.main_window.is_valid());

    /*
     **********************************
     *
     * App initialization order:
     *  1. app launch
     *
     **********************************
     */
    const auto launch_result = app->launch(&ctx);
    if (launch_result != EXIT_SUCCESS)
    {
        destroy_window(ctx.main_window);
        return launch_result;
    }

    /*
     ********************
     *
     * Main loop
     *
     ********************
     */
    while (platform_is_running() && !platform_quit_requested() && !ctx.quit)
    {
        poll_input(&ctx.default_input);
        app->tick(&ctx);
    }

    /*
     *********************************
     *
     * App shutdown order:
     *  1. app shutdown
     *
     *********************************
     */
    app->shutdown(&ctx);

    /*
     *********************************
     *
     * Engine shutdown order
     *  1. GPU destroy
     *  2. platform shutdown
     *
     *********************************
     */

    // Destroy graphics systems
    gpu_destroy();

    if (platform_is_running())
    {
        platform_shutdown(); // closes all windows by default
    }

    return EXIT_SUCCESS;
}


} // namespace bee