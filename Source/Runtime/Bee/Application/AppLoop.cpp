/*
 *  AppLoop.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Application/AppLoop.hpp"
#include "Bee/Graphics/GPU.hpp"

namespace bee {


int app_init(const AppInitInfo& info, AppContext* ctx)
{
    /*
     ************************************
     *
     * Engine initialization order:
     *  0. init reflection etc.
     *  1. platform launch
     *  2. ctx alloc
     *  3. input buffer init
     *  4. gpu init
     *  5. main window create
     *
     ************************************
     */
    if (!platform_launch(info.app_name))
    {
        return EXIT_FAILURE;
    }

    // Initialize platform
    input_buffer_init(&ctx->default_input);

    // Initialize graphics systems
    if (!gpu_init())
    {
        log_error("Failed to initialize GPU backend");
        return EXIT_FAILURE;
    }

    // Create main window
    ctx->main_window = create_window(info.main_window_config);
    BEE_ASSERT(ctx->main_window.is_valid());

    /*
     **********************************
     *
     * App initialization order:
     *  ...
     *
     **********************************
     */
    return EXIT_SUCCESS;
}

void app_shutdown()
{
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

}


} // namespace bee