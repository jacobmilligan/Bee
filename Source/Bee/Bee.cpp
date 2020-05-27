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
#include "Bee/Core/CLI.hpp"

#define BEE_DEFAULT_APP_PLUGIN "Bee.Editor"

using namespace bee;

enum class CommandLineAction
{
    success,
    error,
    help_requested
};

struct BootConfig
{
    String              app_plugin;
    DynamicArray<Path>  search_paths;
};


cli::ParserDescriptor get_parser_descriptor()
{
    static cli::Option options[]
    {
        cli::Option('a', "app-plugin", true, "The application to boot with", 1),
        cli::Option('s', "search-paths", false, "Additional search paths to look for plugins", -1)
    };

    cli::ParserDescriptor desc{};
    desc.options = options;
    desc.option_count = static_array_length(options);
    return desc;
}

CommandLineAction fill_boot_config(const cli::Results& results, BootConfig* config)
{
    if (results.help_requested)
    {
        log_info("%s", results.requested_help_string);
        return CommandLineAction::help_requested;
    }

    if (!results.success)
    {
        log_error("%s", results.error_message.c_str());
        return CommandLineAction::error;
    }

    config->app_plugin = cli::get_option(results, "app-plugin");
    config->search_paths.push_back(fs::get_root_dirs().binaries_root.join("Plugins"));

    const auto search_path_count = cli::get_option_count(results, "search-paths");
    if (search_path_count > 0)
    {
        for (int i = 0; i < search_path_count; ++i)
        {
            config->search_paths.push_back(cli::get_option(results, "search-paths", i));
        }
    }

    return CommandLineAction::success;
}

CommandLineAction parse_command_line(int argc, char** const argv, BootConfig* config)
{
    auto results = cli::parse(argc, argv, get_parser_descriptor());
    return fill_boot_config(results, config);
}

CommandLineAction parse_command_line(int argc, const char** argv, BootConfig* config)
{
    auto results = cli::parse(argc, argv, get_parser_descriptor());
    return fill_boot_config(results, config);
}

CommandLineAction parse_command_line(const char* prog_name, const char* args, BootConfig* config)
{
    auto results = cli::parse(prog_name, args, get_parser_descriptor());
    return fill_boot_config(results, config);
}


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

    // Handle the command line args
    int app_args_index = 0;

    for (int i = 0; i < argc; ++i)
    {
        if (str::compare(argv[i], "--") == 0)
        {
            app_args_index = i + 1;
            break;
        }
    }

    int app_argc = argc - app_args_index;
    char** app_argv = argv + app_args_index;
    BootConfig boot_config;
    CommandLineAction cli_result;

    // if there's no CLI options we need to try and read a boot config from disk or load the default app (Bee.Editor)
    if (app_args_index != 0)
    {
        // If we have extra args then we're specifying an inline boot config `--` and then the app args
        cli_result = parse_command_line(argc - app_args_index, argv, &boot_config);
    }
    else
    {
        const auto boot_config_path = Path::executable_path().parent_path().join("bee.boot");

        if (boot_config_path.exists())
        {
            // Boot config is just the command line in a file
            const auto args = fs::read(boot_config_path);
            cli_result = parse_command_line("Bee", args.c_str(), &boot_config);
        }
        else
        {
            // load the default app
            const char* args[] = {
                "Bee",
                "--app-plugin", BEE_DEFAULT_APP_PLUGIN
            };

            cli_result = parse_command_line(static_array_length(args), args, &boot_config);
        }
    }

    if (cli_result == CommandLineAction::help_requested)
    {
        return EXIT_SUCCESS;
    }

    if (cli_result == CommandLineAction::error)
    {
        return EXIT_FAILURE;
    }

    for (const auto& search_path : boot_config.search_paths)
    {
        plugin_registry.add_search_path(search_path, RegisterPluginMode::manual_load);
    }

    // Load up the app plugin
    if (!plugin_registry.load_plugin(boot_config.app_plugin.view(), plugin_version_any))
    {
        bee::log_error("App plugin %s was not found at any of the plugin search paths", boot_config.app_plugin.c_str());
        return EXIT_FAILURE;
    }

    auto* app = plugin_registry.get_module<ApplicationModule>(BEE_APPLICATION_MODULE_NAME);

    if (app->launch == nullptr)
    {
        bee::log_error("App plugin didn't register an an application module to execute");
        return EXIT_FAILURE;
    }

    const auto launch_result = app->launch(app->instance, app_argc, app_argv);

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