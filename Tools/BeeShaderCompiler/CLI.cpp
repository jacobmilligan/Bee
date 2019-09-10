/*
 *  CLI.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/CLI.hpp"
#include "Bee/Application/Main.hpp"
#include "Bee/ShaderCompiler/BSC.hpp"
#include "Bee/Core/Process.hpp"
#include "Bee/Core/Filesystem.hpp"


static const bee::cli::Positional compile_positionals[] =
{
    bee::cli::Positional("target", "Target shader language to compile to"),
    bee::cli::Positional("destination", "Output directory to place all compiled sources")
};

static const bee::cli::Option compile_options[] =
{
    bee::cli::Option('s', "sources", true, "A list of .bsc source files to compile", -1)
};


int bee_main(int argc, char** argv)
{
    bee::cli::ParserDescriptor compile_parser{};
    compile_parser.command_name = "compile";
    compile_parser.option_count = bee::static_array_length(compile_options);
    compile_parser.options = compile_options;
    compile_parser.positional_count = bee::static_array_length(compile_positionals);
    compile_parser.positionals = compile_positionals;

    bee::cli::ParserDescriptor parser{};
    parser.subparsers = &compile_parser;
    parser.subparser_count = 1;

    const auto cli = bee::cli::parse(argc, argv, parser);
    if (cli.subparsers.find("compile") == nullptr)
    {
        bee::log_error("missing compile option");
        return EXIT_FAILURE;
    }

    bee::socket_startup();

    bee::SocketAddress bsc_addr{};
    bee::socket_reset_address(&bsc_addr, bee::SocketType::tcp, bee::SocketAddressFamily::ipv4, BEE_IPV4_LOCALHOST, BSC_DEFAULT_PORT);
    auto client = bee::bsc_connect_client(bsc_addr);

    // Start a new server instance if we failed to connect to the server
    if (client == bee::socket_t{})
    {
        auto server_path = bee::fs::get_appdata().binaries_root.join("BSCServer");
#if BEE_OS_WINDOWS == 1
        server_path.set_extension(".exe");
#endif // BEE_OS_WINDOWS == 1

        bee::ProcessHandle proc_handle{};
        bee::CreateProcessInfo proc_info{};
        proc_info.handle = &proc_handle;
        proc_info.flags = bee::CreateProcessFlags::create_hidden;
        proc_info.program = server_path.c_str();
        bee::create_process(proc_info);

        bee::log_info("BSC: Starting new server instance");
        client = bee::bsc_connect_client(bsc_addr); // reconnect now that the server is running
    }

    if (!bee::bsc_shutdown_server(client))
    {
        bee::log_warning("Failed to send shutdown command");
    }

    if (bee::socket_shutdown(client))
    {
        bee::socket_close(client);
    }

    bee::socket_cleanup();

    return EXIT_SUCCESS;
}