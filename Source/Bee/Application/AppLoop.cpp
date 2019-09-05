/*
 *  AppLoop.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Application/AppLoop.hpp"

namespace bee {


int app_loop(const AppLaunchConfig& config, Application* app)
{
    if (!platform_launch(config.app_name))
    {
        return EXIT_FAILURE;
    }

    AppContext ctx;

    input_buffer_init(&ctx.default_input);

    ctx.main_window = create_window(config.main_window_config);
    BEE_ASSERT(ctx.main_window.is_valid());

    const auto launch_result = app->launch(&ctx);
    if (launch_result != EXIT_SUCCESS)
    {
        destroy_window(ctx.main_window);
        return launch_result;
    }

    while (platform_is_running() && !platform_quit_requested() && !ctx.quit)
    {
        poll_input(&ctx.default_input);
        app->tick(&ctx);
    }

    app->shutdown(&ctx);

    if (platform_is_running())
    {
        platform_shutdown(); // closes all windows by default
    }

    return EXIT_SUCCESS;
}


} // namespace bee