/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetCompiler/Connection.hpp"
#include "Bee/Application/Main.hpp"
#include "Bee/Core/CLI.hpp"

#include <string.h>


int run_server()
{
    bee::AssetCompilerConnection connection{};
    bee::SocketAddress address{};

    const auto result = bee::socket_reset_address(&address, bee::SocketType::tcp, bee::SocketAddressFamily::ipv4, BEE_IPV4_LOCALHOST, BEE_AC_DEFAULT_PORT);
    if (result != BEE_SOCKET_SUCCESS)
    {
        bee::log_error("Bee Asset Compiler: socket address error: %s", bee::socket_code_to_string(result));
        return EXIT_FAILURE;
    }

    // TODO(Jacob): think of a good allocator strategy for messages
    if (!bee::asset_compiler_listen(address, &connection))
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


int run_client()
{
    bee::AssetCompilerConnection connection{};
    bee::SocketAddress address{};

    const auto result = bee::socket_reset_address(&address, bee::SocketType::tcp, bee::SocketAddressFamily::ipv4, BEE_IPV4_LOCALHOST, BEE_AC_DEFAULT_PORT);
    if (result != BEE_SOCKET_SUCCESS)
    {
        bee::log_error("Bee Asset Compiler: socket address error: %s", bee::socket_code_to_string(result));
        return EXIT_FAILURE;
    }

    // TODO(Jacob): think of a good allocator strategy for messages
    if (!bee::asset_compiler_connect(address, &connection))
    {
        return EXIT_FAILURE;
    }

    bee::cli::ParserDescriptor subparsers[4];

    bee::cli::Positional load_plugin_positionals[] = {
        bee::cli::Positional("directory", "The directory where the plugin library file is located"),
        bee::cli::Positional("filename", "The plugins library file name")
    };
    bee::cli::Positional unload_plugin_positional("name", "The name of the plugin to unload (not the library filename)");
    bee::cli::Positional compile_positionals[] = {
        bee::cli::Positional("platform", "The platform to compile for. Must be a valid bee::AssetPlatform mask"),
        bee::cli::Positional("source", "The path to the source file to compile"),
        bee::cli::Positional("destination", "The relative path that the source files compilation results should be output to"),
    };

    bee::cli::Option shutdown_option('c', "client-only", false, "If used, shuts down the client only, leaving the server running", 0);

    subparsers[0].command_name = "shutdown";
    subparsers[0].option_count = 1;
    subparsers[0].options = &shutdown_option;

    subparsers[1].command_name = "load-plugin";
    subparsers[1].positional_count = bee::static_array_length(load_plugin_positionals);
    subparsers[1].positionals = load_plugin_positionals;

    subparsers[2] = subparsers[1];
    subparsers[2].command_name = "unload-plugin";
    subparsers[2].positional_count = 1;
    subparsers[2].positionals = &unload_plugin_positional;

    subparsers[3].command_name = "compile";
    subparsers[3].positional_count = bee::static_array_length(compile_positionals);
    subparsers[3].positionals = compile_positionals;

    bee::cli::ParserDescriptor parser{};
    parser.subparser_count = bee::static_array_length(subparsers);
    parser.subparsers = subparsers;

    bee::cli::Results cli;
    bool is_running = true;
    char read_buffer[4096];

    while (is_running)
    {
        const auto command_line = fgets(read_buffer, bee::static_array_length(read_buffer), stdin);
        cli = bee::cli::parse("bac", command_line, parser, bee::temp_allocator());

        if (!cli.success)
        {
            bee::log_error("%s", cli.error_message.c_str());
            continue;
        }

        if (cli.help_requested)
        {
            bee::log_info("%s", cli.requested_help_string);
            continue;
        }

        const auto shutdown_cmd = cli.subparsers.find("shutdown");
        const auto load_plugin_cmd = cli.subparsers.find("load-plugin");
        const auto unload_plugin_cmd = cli.subparsers.find("unload-plugin");
        const auto compile_cmd = cli.subparsers.find("compile");

        if (shutdown_cmd != nullptr)
        {
            if (!bee::cli::has_option(shutdown_cmd->value, "client-only"))
            {
                // Also shutdown the server
                bee::ACShutdownMsg msg{};
                bee::asset_compiler_send_message(connection, msg);
            }

            is_running = false;
            continue;
        }

        if (load_plugin_cmd != nullptr)
        {
            const auto plugin_dir = bee::cli::get_positional(load_plugin_cmd->value, 0);
            const auto plugin_filename = bee::cli::get_positional(load_plugin_cmd->value, 1);
            bee::ACLoadPluginMsg msg{};
            bee::str::copy(msg.directory, bee::static_array_length(msg.directory), plugin_dir);
            bee::str::copy(msg.filename, bee::static_array_length(msg.filename), plugin_filename);
            bee::asset_compiler_send_message(connection, msg);

            if (bee::asset_compiler_wait_last_message(connection))
            {
                bee::log_info("Bee Asset Compiler: loaded plugin: %s", plugin_filename);
            }

            continue;
        }

        if (unload_plugin_cmd != nullptr)
        {
            const auto plugin_name = bee::cli::get_positional(unload_plugin_cmd->value, 0);
            bee::ACUnloadPluginMsg msg{};
            bee::str::copy(msg.name, bee::static_array_length(msg.name), plugin_name);
            bee::asset_compiler_send_message(connection, msg);

            if (bee::asset_compiler_wait_last_message(connection))
            {
                bee::log_info("Bee Asset Compiler: unloaded plugin: %s", plugin_name);
            }

            continue;
        }

        if (compile_cmd != nullptr)
        {
            const auto platform_string = bee::cli::get_positional(compile_cmd->value, 0);
            const auto platform = strtoul(platform_string, nullptr, 0);
            if (platform == ULONG_MAX)
            {
                bee::log_error("Bee Asset Compiler: invalid AssetPlatform mask: %s", platform_string);
                continue;
            }

            auto src_path = bee::cli::get_positional(compile_cmd->value, 1);
            auto dst_path = bee::cli::get_positional(compile_cmd->value, 2);

            bee::ACCompileMsg msg{};
            msg.platform = static_cast<bee::AssetPlatform>(platform);
            bee::str::copy(msg.src_path, bee::static_array_length(msg.src_path), src_path);
            bee::str::copy(msg.dst_path, bee::static_array_length(msg.dst_path), dst_path);
            bee::asset_compiler_send_message(connection, msg);

            if (bee::asset_compiler_wait_last_message(connection))
            {
                bee::log_info("Bee Asset Compiler: compiled \"%s\" successfully", msg.src_path);
            }
            else
            {
                bee::log_error("Bee Asset Compiler: failed to compile \"%s\"", msg.src_path);
            }
        }
    }

    bee::ACShutdownMsg msg{};
    bee::asset_compiler_send_message(connection, msg);
    return EXIT_SUCCESS;
}


int bee_main(int argc, char** argv)
{
    bee::cli::Positional type_positional("type", "The connection type to use for the app. One of: client, server");
    bee::cli::ParserDescriptor parser{};
    parser.positional_count = 1;
    parser.positionals = &type_positional;

    const auto cli = bee::cli::parse(argc, argv, parser);
    if (!cli.success)
    {
        bee::log_error("%s", cli.error_message.c_str());
        return EXIT_FAILURE;
    }

    if (cli.help_requested)
    {
        bee::log_info("%s", cli.requested_help_string);
        return EXIT_SUCCESS;
    }

    const auto connection_type = bee::cli::get_positional(cli, 0);
    const auto is_server = bee::str::compare(connection_type, "server") == 0;
    const auto is_client = bee::str::compare(connection_type, "client") == 0;

    if (!is_server && !is_client)
    {
        bee::log_error("bac: Invalid connection type \"%s\". Must be one of: client, server", connection_type);
        return EXIT_FAILURE;
    }

    bee::socket_startup();
    int result;

    if (is_server)
    {
        result = run_server();
    }
    else
    {
        result = run_client();
    }

    bee::socket_cleanup();

    return result;
}