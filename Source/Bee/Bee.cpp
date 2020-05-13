/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Bee.hpp"

#include "Bee/Core/Main.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Graphics/GPU.hpp"


using namespace bee;

int bee_main(int argc, char** argv)
{
    // Initialize core systems before launching the platform and plugin registry
    bee::JobSystemInitInfo job_system_info{};
    job_system_init(job_system_info);

    // Initialize graphics systems
    if (!bee::gpu_init())
    {
        bee::log_error("Failed to initialize GPU backend");
        return EXIT_FAILURE;
    }

    // Ensure plugin registry is initialized before anything else
    bee::PluginRegistry plugin_registry;
    plugin_registry.add_search_path(bee::fs::get_appdata().binaries_root.join("Plugins"), RegisterPluginMode::auto_load);

    auto* app = plugin_registry.get_module<ApplicationModule>(BEE_APPLICATION_MODULE_NAME);

    if (app->launch == nullptr)
    {
        bee::log_error("Couldn't find an application module to execute");
        return EXIT_FAILURE;
    }

    const auto launch_result = app->launch(app->instance, argc, argv);

    if (launch_result != EXIT_SUCCESS)
    {
        app->fail(app->instance);
        return launch_result;
    }

    while (true)
    {
        plugin_registry.refresh_plugins();

        const auto state = app->tick(app->instance);

        if (state == ApplicationState::quit_requested)
        {
            app->shutdown(app->instance);
            break;
        }
    }

    /*
     * shutdown plugin registry before core systems to ensure that all core systems are available
     * if a plugin has to use one in its unload function
     */
    bee::destruct(&plugin_registry);

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