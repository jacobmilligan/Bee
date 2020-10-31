/*
 *  Plugin.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Sandbox.hpp"

#include "Bee/Core/Main.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Filesystem.hpp"


int bee_main(int argc, char** argv)
{
    bee::JobSystemInitInfo job_system_info{};
    bee::job_system_init(job_system_info);

    bee::init_plugins();
    bee::log_info("%s\n", bee::fs::get_root_dirs().binaries_root.join("Plugins").c_str());
    add_plugin_search_path(bee::fs::get_root_dirs().binaries_root.join("Plugins"));
    bee::refresh_plugins();
    bee::load_plugin("Bee.Sandbox");

    auto* sandbox = static_cast<bee::SandboxModule*>(bee::get_module(BEE_SANDBOX_MODULE_NAME));

    if (!sandbox->startup())
    {
        sandbox->shutdown();
        return EXIT_FAILURE;
    }

    while (true)
    {
        bee::refresh_plugins();
        if (!sandbox->tick())
        {
            break;
        }
    }

    sandbox->shutdown();
    bee::job_system_shutdown();
    bee::shutdown_plugins();

    return EXIT_SUCCESS;
}
