/*
 *  Client.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/ShaderCompiler/Commands.hpp"
#include "Bee/Core/Socket.hpp"
#include "Bee/Core/Logger.hpp"

namespace bee {


template <typename CmdType>
bool bsc_send_command(const socket_t client, const CmdType& cmd)
{
    static constexpr auto send_length = sizeof(CmdType) + sizeof(BSCCommandType);

    char send_buffer[send_length];
    memcpy(send_buffer, &cmd.header, sizeof(BSCCommandType));
    memcpy(send_buffer, &cmd, sizeof(CmdType));

    auto result = socket_send(client, send_buffer, send_length);

    BSCCommandType complete = BSCCommandType::unknown;
    int recv_count = 0;
    int read_size = 0;
    do
    {
        recv_count = socket_recv(client, reinterpret_cast<char*>(&complete) + read_size, sizeof(BSCCommandType));
        read_size += recv_count;
    } while (recv_count > 0 && read_size < sizeof(BSCCommandType));

    return result == send_length && read_size == sizeof(BSCCommandType) && complete == BSCCommandType::complete;
}

socket_t bsc_connect_client(const SocketAddress& address)
{
    socket_t client{};
    if (!socket_open(&client, address))
    {
        socket_close(client);
        return socket_t{};
    }

    if (!socket_connect(&client, address))
    {
        socket_close(client);
        return socket_t{};
    }
    
    return client;
}

bool bsc_shutdown_server(const socket_t client, const bool immediate)
{
    BSCShutdownCmd cmd{};
    cmd.immediate = immediate;
    return bsc_send_command(client, cmd);
}


} // namespace bee