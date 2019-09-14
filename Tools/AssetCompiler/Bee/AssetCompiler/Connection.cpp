/*
 *  Server.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetCompiler/Connection.hpp"
#include "Bee/Core/Serialization/MemorySerializer.hpp"
#include "Bee/AssetCompiler/Pipeline.hpp"
#include "Bee/AssetCompiler/Messages.hpp"


namespace bee {


/*
 ********************************************
 *
 * # Asset Compiler API - implementation
 *
 ********************************************
 */


enum class ACServerReadResult
{
    shutdown,
    error,
    success
};


struct ACClient
{
    ACConnectionType    connection_type { ACConnectionType::not_connected };
    socket_t            socket { 0 };
    char                temp_msg_buffer[16];
    i32                 temp_msg_buffer_offset { 0 };
    DynamicArray<u8>    message_buffer;
};

bool asset_compiler_recv(const socket_t socket, char* buffer, const i32 buffer_max, const i32 read_size)
{
    if (BEE_FAIL(read_size <= buffer_max))
    {
        return false;
    }

    int bytes_read = 0;
    int recv_count = 0;

    do
    {
        recv_count = socket_recv(socket, buffer + bytes_read, read_size - bytes_read);
        bytes_read += recv_count;
    } while (recv_count > 0 && bytes_read < read_size);

    // Handle errors with the recv loop
    if (socket_code_to_error(recv_count) == SocketError::connection_reset_by_peer)
    {
        log_info("Host disconnected: %llu", socket);
        socket_close(socket);
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

    return true;
}

bool asset_compiler_wait_last_message(const AssetCompilerConnection& connection)
{
    // Read the id and then the message size
    static constexpr i32 header_size = sizeof(ACMessageId) + sizeof(i32);
    char temp_buffer[header_size];
    if (!asset_compiler_recv(connection.socket, temp_buffer, static_array_length(temp_buffer), header_size))
    {
        log_error("Bee Asset Compiler: client failed to read header");
        return false;
    }

    const auto id = *reinterpret_cast<ACMessageId*>(temp_buffer);
    const auto size = *reinterpret_cast<i32*>(temp_buffer + sizeof(ACMessageId));

    if (id != ACMessageId::complete)
    {
        return false;
    }

    if (size > sizeof(char) * 4)
    {
        log_error("Bee Asset Compiler: invalid message size");
        return false;
    }

    char msg[4];
    const auto msg_read_success = asset_compiler_recv(
        connection.socket, msg, static_array_length(msg), static_array_length(msg)
    );

    if (!msg_read_success)
    {
        return false;
    }

    return *reinterpret_cast<bool*>(msg);
}


/*
 ********************************************
 *
 * # Asset Compiler Server
 *
 ********************************************
 */
ACServerReadResult asset_compiler_server_read(AssetPipeline* pipeline, fd_set_t* read_set, ACClient* clients, const i32 client_count);


bool asset_compiler_listen(const SocketAddress& address, AssetCompilerConnection* connection, Allocator* message_allocator)
{
    auto result = socket_open(&connection->socket, address);
    if (result != BEE_SOCKET_SUCCESS)
    {
        log_error("Bee Asset Compiler: failed to open socket for server: %s", socket_code_to_string(result));
        return false;
    }

    result = socket_bind(connection->socket, address);
    if (result != BEE_SOCKET_SUCCESS)
    {
        log_error("Bee Asset Compiler: failed to bind server socket to address: %s", socket_code_to_string(result));
        return false;
    }

    result = socket_listen(connection->socket, BEE_AC_MAX_CLIENTS);
    if (result != BEE_SOCKET_SUCCESS)
    {
        log_error("Bee Asset Compiler: server failed to listen on address %s: %s", address.to_string(), socket_code_to_string(result));
        return false;
    }

    connection->connection_type = ACConnectionType::server;
    connection->message_allocator = message_allocator;

    log_info("Bee Asset Compiler: listening on %s", address.to_string());

    timeval timeout{};
    timeout.tv_sec = 180; // timeout on the select to make sure we don't get indefinitely stuck waiting for a connection

    fd_set_t read_set;
    ACClient clients[BEE_AC_MAX_CLIENTS];
    for (auto& client : clients)
    {
        new (&client.message_buffer) DynamicArray<u8>(message_allocator);
    }

    // create a new pipeline for the server
    AssetPipeline pipeline;

    while (true)
    {
        if (connection->connection_type != ACConnectionType::server)
        {
            break;
        }

        socket_fd_zero(&read_set);
        socket_fd_set(connection->socket, &read_set);

        for (const auto& client : clients)
        {
            if (client.socket > 0)
            {
                socket_fd_set(client.socket, &read_set);
            }
        }

        const auto connection_count = socket_select(connection->socket, &read_set, nullptr, nullptr, timeout);

        // an error occurred
        if (connection_count < 0)
        {
            log_error("Bee Asset Compiler: server socket select error: %s", socket_code_to_string(connection_count));
            return false;
        }

        // A timeout occurred without any connections
        if (connection_count == 0)
        {
            log_info("Bee Asset Compiler: timed out waiting for client connections");
        }

        // if the server socket is in the read set it indicates at least one connection was requested
        const auto has_new_connections = socket_fd_isset(connection->socket, &read_set);
        socket_t new_connection{};
        if (has_new_connections)
        {
            result = socket_accept(connection->socket, &new_connection);
            if (result != BEE_SOCKET_SUCCESS)
            {
                log_error("Bee Asset Compiler: server failed to accept client: %s", socket_code_to_string(result));
                continue;
            }

            for (auto& client : clients)
            {
                if (client.connection_type == ACConnectionType::not_connected)
                {
                    client.socket = new_connection;
                    client.connection_type = ACConnectionType::client;
                    break;
                }
            }
        }

        const auto recv_result = asset_compiler_server_read(&pipeline, &read_set, clients, static_array_length(clients));
        if (recv_result == ACServerReadResult::shutdown)
        {
            break;
        }
    }

    result = socket_close(connection->socket);
    if (result != BEE_SOCKET_SUCCESS)
    {
        log_error("Bee Asset Compiler: failed to shutdown server: %s", socket_code_to_string(result));
    }
    connection->socket = 0;
    return true;
}


ACServerReadResult asset_compiler_server_read(AssetPipeline* pipeline, fd_set_t* read_set, ACClient* clients, const i32 client_count)
{
    auto msg_id = ACMessageId::unknown;
    i32 msg_size = 0;

    for (int c = 0; c < client_count; ++c)
    {
        auto& client = clients[c];

        if (!socket_fd_isset(client.socket, read_set))
        {
            continue; // socket did nothing in the last select call
        }

        // Read the id and then the message size
        static constexpr i32 header_size = sizeof(ACMessageId) + sizeof(i32);
        if (!asset_compiler_recv(client.socket, client.temp_msg_buffer, static_array_length(client.temp_msg_buffer), header_size))
        {
            log_error("Bee Asset Compiler: server failed to read header");
            continue;
        }

        msg_id = *reinterpret_cast<ACMessageId*>(client.temp_msg_buffer);
        msg_size = *reinterpret_cast<i32*>(client.temp_msg_buffer + sizeof(ACMessageId));

        client.message_buffer.resize(msg_size);
        const auto msg_read_success = asset_compiler_recv(
            client.socket,
            reinterpret_cast<char*>(client.message_buffer.data()),
            client.message_buffer.capacity(),
            client.message_buffer.size()
        );

        // Try and read the full message
        if (!msg_read_success)
        {
            log_error("Bee Asset Compiler: server failed to read message");
            continue;
        }

        MemorySerializer serializer(&client.message_buffer);
        switch (msg_id)
        {
            case ACMessageId::shutdown:
            {
                return ACServerReadResult::shutdown;
            }

            case ACMessageId::load_plugin:
            {
                ACLoadPluginMsg msg{};
                serialize(SerializerMode::reading, &serializer, &msg);
                pipeline->load_plugin(msg.directory, msg.filename);
                break;
            }

            case ACMessageId::unload_plugin:
            {
                ACUnloadPluginMsg msg{};
                serialize(SerializerMode::reading, &serializer, &msg);
                pipeline->unload_plugin(msg.name);
                break;
            }

            case ACMessageId::compile:
            {
                ACCompileMsg msg{};
                serialize(SerializerMode::reading, &serializer, &msg);
                pipeline->compile(msg.platform, msg.src_path, msg.dst_path);
                break;
            }

            default:
            {
                log_error("Bee Asset Compiler: unknown message parsed");
                break;
            }
        }
    }

    return ACServerReadResult::success;
}


/*
 ********************************************
 *
 * # Asset Compiler Client
 *
 ********************************************
 */
bool asset_compiler_connect(const SocketAddress& address, AssetCompilerConnection* connection, Allocator* message_allocator)
{
    connection->connection_type = ACConnectionType::not_connected;
    connection->message_allocator = message_allocator;

    while (connection->socket != 0) {} // wait for server to shutdown if we're switching from client to server

    auto result = socket_open(&connection->socket, address);
    if (result != BEE_SOCKET_SUCCESS)
    {
        log_error("Bee Asset Compiler: failed to open client socket: %s", socket_code_to_string(result));
        socket_close(connection->socket);
        connection->socket = 0;
        return false;
    }

    result = socket_connect(&connection->socket, address);
    if (result != BEE_SOCKET_SUCCESS)
    {
        log_error("Bee Asset Compiler: failed to connect client to server: %s", socket_code_to_string(result));
        socket_close(connection->socket);
        connection->socket = 0;
        return false;
    }

    connection->connection_type = ACConnectionType::client;
    return true;
}


template <typename MsgType>
bool asset_compiler_client_send(const AssetCompilerConnection& client, const MsgType& msg)
{
    /*
     * Commands are sent to the bee shader compiler server like this:
     * | id | cmd size | cmd data |
     */
    auto msg_allocator = client.message_allocator == nullptr ? system_allocator() : client.message_allocator;
    DynamicArray<u8> send_buffer(sizeof(ACMessageId) + sizeof(i32), msg_allocator);

    // Allocate enough memory for the size and header info
    // Copy header first
    memcpy(send_buffer.data(), &msg.id, sizeof(ACMessageId));


    // Serialize the command data
    MemorySerializer serializer(&send_buffer);
    serialize(SerializerMode::writing, &serializer, &msg);

    // Copy the total command size minus sizeof the id and size info
    const auto msg_size = send_buffer.size() - sizeof(i32) - sizeof(ACMessageId);
    memcpy(send_buffer.data() + sizeof(ACMessageId), &msg_size, sizeof(i32));

    return socket_send(client.socket, reinterpret_cast<char*>(send_buffer.data()), send_buffer.size()) == send_buffer.size();
}


bool asset_compiler_shutdown_server(const AssetCompilerConnection& client)
{
    ACShutdownMsg msg{};
    return asset_compiler_client_send(client, msg);
}


} // namespace bee