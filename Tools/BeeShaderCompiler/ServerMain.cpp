/*
 *  BSCServer.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Application/Main.hpp"
#include "Bee/ShaderCompiler/BSC.hpp"

int bee_main(int argc, char** argv)
{
    bee::socket_startup();
    bee::SocketAddress addr{};
    bee::socket_reset_address(&addr, bee::SocketType::tcp, bee::SocketAddressFamily::ipv4, BEE_IPV4_LOCALHOST, BSC_DEFAULT_PORT);
    const auto result = bee::bsc_server_listen(addr);
    bee::socket_cleanup();
    return result;
}