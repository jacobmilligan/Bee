/*
 *  Engine.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "BeeGame/Game.hpp"

namespace bee {


int game_run(const AppInitInfo& info, const char* game_plugin_name)
{
    AppContext ctx{};
    const auto init_result = app_init(info, &ctx);
    if (init_result != EXIT_SUCCESS)
    {
        return init_result;
    }

    while (!ctx.quit)
    {
        poll_input(&ctx.default_input);
    }

    app_shutdown();
    return EXIT_SUCCESS;
}


} // namespace bee