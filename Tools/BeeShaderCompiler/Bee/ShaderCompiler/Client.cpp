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
    auto send_buffer = DynamicArray<u8>::with_size(sizeof(BSCCommandType));
    memcpy(send_buffer.data(), &cmd.header, sizeof(BSCCommandType));

    MemorySerializer serializer(&send_buffer);
    serialize(SerializerMode::writing, &serializer, &cmd);

    auto result = socket_send(client, reinterpret_cast<char*>(send_buffer.data()), send_buffer.size());

    BSCCommandType complete = BSCCommandType::unknown;
    int recv_count = 0;
    int read_size = 0;
    do
    {
        recv_count = socket_recv(client, reinterpret_cast<char*>(&complete) + read_size, sizeof(BSCCommandType));
        read_size += recv_count;
    } while (recv_count > 0 && read_size < sizeof(BSCCommandType));

    return result == send_buffer.size() && read_size == sizeof(BSCCommandType) && complete == BSCCommandType::complete;
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

bool bsc_compile(const socket_t client, const BSCTarget target, const i32 source_count, const Path* source_paths, BSCModule* dst_modules)
{
    BSCCompileCmd cmd{};
    cmd.target = target;
    cmd.source_paths = FixedArray<Path>::with_size(source_count);
    for (int i = 0; i < cmd.source_paths.size(); ++i)
    {
        cmd.source_paths[i] = source_paths[i];
    }
    return bsc_send_command(client, cmd);
}


} // namespace bee