/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Bee.hpp"
#include "Bee/Core/CLI.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"
#include "Bee/Editor/EditorApp.hpp"


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

    if (parser.help_requested)
    {
        log_info("%s", parser.requested_help_string);
        return EXIT_SUCCESS;
    }

    // Handle open command
    const auto open_results = parser.subparsers.find("open-project");
    const auto create_results = parser.subparsers.find("create-project");

    EditorLaunchParameters params{};

    if (open_results != nullptr)
    {
        const auto& results = open_results->value;

        if (cli::has_option(results, "platform"))
        {
            params.platform = enum_from_string<AssetPlatform>(cli::get_option(results, "platform"));
        }

        params.mode = EditorLaunchMode::new_project;
        params.project_path = cli::get_positional(results, 0);
    }
    else if (create_results != nullptr)
    {
        const auto& results = create_results->value;

        params.mode = EditorLaunchMode::new_project;
        params.project_name = cli::get_positional(results, 0);
        params.project_path = cli::get_positional(results, 1);

        if (cli::has_option(results, "platform"))
        {
            params.platform = enum_from_string<AssetPlatform>(cli::get_option(results, "platform"));
        }
    }

    return editor_app_run(params);
}