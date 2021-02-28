/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#include "Bee/Editor/App.hpp"

#include "Bee/Core/Main.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Filesystem.hpp"


int bee_main(int argc, char** argv)
{
    bee::JobSystemInitInfo job_system_info{};
    bee::job_system_init(job_system_info);

    bee::init_plugins();

    add_plugin_search_path(bee::fs::roots().binaries.join("Plugins").view());
    add_plugin_source_path(bee::fs::roots().sources.view());

    bee::refresh_plugins();
    bee::load_plugin("Bee.Editor");

    auto* editor = static_cast<bee::EditorAppModule*>(bee::get_module(BEE_EDITOR_APP_MODULE_NAME));

    if (!editor->startup())
    {
        editor->shutdown();
        return EXIT_FAILURE;
    }

    while (!editor->quit_requested())
    {
        bee::temp_allocator_reset();
        bee::refresh_plugins();
        editor->tick();
    }

    editor->shutdown();
    bee::job_system_shutdown();
    bee::shutdown_plugins();

    return EXIT_SUCCESS;
}