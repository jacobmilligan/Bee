/*
 *  Server.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/ShaderCompiler/BSC.hpp"
#include "Bee/ShaderCompiler/Compiler.hpp"
#include "Bee/Core/Logger.hpp"

namespace bee {


enum class BSCRecvResult
{
    shutdown,
    error,
    success
};


struct BSCClient
{
    socket_t            socket;
    DynamicArray<u8>    recv_buffer;
};


static struct BSCServer
{
    socket_t    socket;
    fd_set_t    read_set;
    BSCClient   clients[BSC_MAX_CLIENTS];
    BSCCompiler compiler;
} g_server;



bool bsc_server_read(BSCClient* client, const i32 read_size)
{
    if (client->recv_buffer.size() < read_size)
    {
        client->recv_buffer.resize(read_size);
    }

    int bytes_read = 0;
    int recv_count = 0;

    do
    {
        recv_count = socket_recv(client->socket, reinterpret_cast<char*>(client->recv_buffer.data() + bytes_read), read_size - bytes_read);
        bytes_read += recv_count;
    } while (recv_count > 0 && bytes_read < read_size);

    // Handle errors with the recv loop
    if (WSAGetLastError() == WSAECONNRESET)
    {
        log_info("Host disconnected: %llu", client->socket);
        socket_close(client->socket);
        client->socket = 0;
        return false;
    }

    if (recv_count < 0)
    {
        log_error("Recv failed");
        return false;
    }

    if (recv_count == 0)
    {
        log_info("Disconnected client");
    }

    // all recv completed
    BSCCommandType complete = BSCCommandType::complete;
    socket_send(client->socket, reinterpret_cast<const char*>(&complete), sizeof(BSCCommandType));

    return true;
}


#define BSC_PROCESS_CMD(command_struct_name)                                                                            \
    case command_struct_name::type:                                                                                     \
    {                                                                                                                   \
        const auto read_success = bsc_server_read(&client, sizeof(command_struct_name));                                \
        if (!read_success || !g_server.compiler.process_command(*reinterpret_cast<const command_struct_name*>(client.recv_buffer)))   \
        {                                                                                                               \
            return BSCRecvResult::error;                                                                                \
        }                                                                                                               \
    } break


BSCRecvResult bsc_server_recv()
{
    // TODO(Jacob): handle the message
    BSCCommandType header;
    i32 read_size = 0;
    bool deferred_shutdown = false;

    for (auto& client : g_server.clients)
    {
        if (!socket_fd_isset(client.socket, &g_server.read_set))
        {
            continue; // client did nothing in the last select call
        }

        if (!bsc_server_read(&client, sizeof(BSCCommandType)))
        {
            return BSCRecvResult::error;
        }

        header = *reinterpret_cast<BSCCommandType*>(client.recv_buffer.data());
        MemorySerializer serializer(&client.recv_buffer);
        switch (header)
        {
            case BSCShutdownCmd::type:
            {
                BSCShutdownCmd cmd{};
                serialize(SerializerMode::reading, &serializer, &cmd);
                break;
            }
            BSC_PROCESS_CMD(BSCShutdownCmd);
            default: break;
        }

        if (g_server.compiler.shutdown_deferred())
        {
            deferred_shutdown = true;
        }

        if (g_server.compiler.shutdown_immediate())
        {
            return BSCRecvResult::shutdown;
        }

        g_server.compiler.reset();
    }

    return deferred_shutdown ? BSCRecvResult::shutdown : BSCRecvResult::success;
}

int bsc_server_listen(const SocketAddress& address)
{
    const auto open_result = socket_open(&g_server.socket, address);
    if (BEE_FAIL_F(open_result, "Failed to launch BeeShaderCompiler server"))
    {
        return EXIT_FAILURE;
    }

    socket_bind(g_server.socket, address);
    socket_listen(g_server.socket, BSC_MAX_CLIENTS);

    log_info("BSC: listening on %s", address->ai_canonname);

    memset(g_server.clients, 0, sizeof(BSCClient) * BSC_MAX_CLIENTS);

    timeval timeout{};
    timeout.tv_sec = 180; // timeout on the select to make sure we don't get indefinitely stuck waiting for a connection

    while (true)
    {
        socket_fd_zero(&g_server.read_set);
        socket_fd_set(g_server.socket, &g_server.read_set);

        // Check if we've received a connection and add to the current read set
        for (const auto& client : g_server.clients)
        {
            if (client.socket > 0)
            {
                socket_fd_set(client.socket, &g_server.read_set);
            }
        }

        const auto ready_count = socket_select(g_server.socket, &g_server.read_set, nullptr, nullptr, timeout);
        if (ready_count < 0)
        {
            return EXIT_FAILURE; // an error occurred
        }

        if (ready_count == 0)
        {
            log_info("BSC: timed out waiting for client connections"); // timeout
            continue;
        }

        const auto handle_new_connections = socket_fd_isset(g_server.socket, &g_server.read_set);
        if (handle_new_connections)
        {
            socket_t connection{};
            if (socket_accept(g_server.socket, &connection)) // accept one incoming connection this iteration
            {
                // Find available client socket to connect using
                for (auto& client : g_server.clients)
                {
                    if (client.socket == 0)
                    {
                        client.socket = connection;
                        break;
                    }
                }
            }
        }

        if (bsc_server_recv() == BSCRecvResult::shutdown)
        {
            break;
        }
    }

    socket_close(g_server.socket);

    return EXIT_SUCCESS;
}


} // namespace bee