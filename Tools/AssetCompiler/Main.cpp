/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetCompiler/Connection.hpp"
#include "Bee/Application/Main.hpp"
#include "Bee/Core/CLI.hpp"


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

    // TODO(Jacob): think of a good allocation strategy for messages
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

    // TODO(Jacob): think of a good allocation strategy for messages
    if (!bee::asset_compiler_connect(address, &connection))
    {
        return EXIT_FAILURE;
    }

    bee::asset_compiler_shutdown_server(connection);
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