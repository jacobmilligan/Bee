/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Application/Main.hpp"
#include "Bee/Core/CLI.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"

#include "BeeEditor/EditorApp.hpp"


using namespace bee;


int bee_main(int argc, char** argv)
{
    /*
     * Initial info to open the editor with
     */
    Project current_project;

    /*
     * positionals & options common to all project subcommands
     */
    cli::Positional open_positionals[] =
    {
        { "location", "Full path to the folder containing the .beeproj.json file to open" }
    };

    cli::Positional create_positionals[] =
    {
        { "name", "The new projects name" },
        { "location", "Full path to the folder containing the .beeproj.json file to open" }
    };

    cli::Option project_options[] =
    {
        { 'p', "platform", false, "The default platform to use for the project", 1 }
    };

    /*
     * Root parser
     */
    cli::ParserDescriptor subcommands[] =
    {
        { "open-project", open_positionals, project_options },
        { "create-project", create_positionals, project_options }
    };

    cli::ParserDescriptor cmd_parser("bee", subcommands);

    const auto parser = cli::parse(argc, argv, cmd_parser);

    // Handle open command
    const auto open_results = parser.subparsers.find("open-project");
    const auto create_results = parser.subparsers.find("create-project");

    if (open_results != nullptr)
    {
        const auto& results = open_results->value;
        const auto location = cli::get_positional(results, 0);

        AssetPlatform platform{};
        if (cli::has_option(results, "platform"))
        {
            platform = enum_from_string<AssetPlatform>(cli::get_option(results, "platform"));
        }

        if (!open_project(&current_project, location, platform))
        {
            return EXIT_FAILURE;
        }
    }
    else if (create_results != nullptr)
    {
        const auto& results = create_results->value;
        current_project.engine_version = BEE_VERSION;
        current_project.name = cli::get_positional(results, 0);
        current_project.location = cli::get_positional(results, 1);
        current_project.platform = default_asset_platform;

        if (cli::has_option(results, "platform"))
        {
            current_project.platform = enum_from_string<AssetPlatform>(cli::get_option(results, "platform"));
        }

        if (!init_project(current_project))
        {
            return EXIT_FAILURE;
        }
    }

    return editor_app_run(&current_project);
}