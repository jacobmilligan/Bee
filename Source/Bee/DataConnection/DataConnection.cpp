/*
 *  MessageServer.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#include "Bee/DataConnection/DataConnection.hpp"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Memory/LinearAllocator.hpp"


namespace bee {


struct ThreadData
{
    LinearAllocator     packet_allocator;
    DynamicArray<u8>    send_buffer;
    i32                 flush_offset { 0 };

    ThreadData()
        : packet_allocator(megabytes(2), system_allocator())
    {}
};

struct Client
{
    bool        in_use { false };
    socket_t    socket;
};

struct DataConnection
{
    static constexpr i32 default_port = 8888;
    static constexpr i32 max_clients = 16;

    DataConnectionFlags                 flags { DataConnectionFlags::invalid };
    SocketAddress                       address;
    socket_t                            socket;
    FixedArray<ThreadData>              thread_data;
    DynamicArray<DataConnectionPacket>  recv_packets;
    DynamicArray<u8>                    recv_buffer;

    // Server data
    Client                  clients[max_clients];
    fd_set_t                read_set;

    ThreadData& get_thread()
    {
        return thread_data[job_worker_id()];
    }

    void clear_recv_buffers()
    {
        recv_packets.clear();
        recv_buffer.clear();
    }
};

Result<void, DataConnectionError> startup()
{
    auto res = socket_startup();
    if (!res)
    {
        return DataConnectionError(res);
    }
    return {};
}

Result<void, DataConnectionError> shutdown()
{
    auto res = socket_cleanup();
    if (!res)
    {
        return DataConnectionError(res);
    }
    return {};
}


Result<DataConnection*, DataConnectionError> create_server(const SocketAddressFamily address_family, const char* hostname, const port_t port)
{
    SocketAddress address{};
    auto res = socket_reset_address(&address, SocketType::tcp, address_family, hostname, port);
    if (!res)
    {
        return DataConnectionError(res);
    }

    socket_t socket{};
    res = socket_open(&socket, address);
    if (!res)
    {
        return DataConnectionError(res);
    }

    res = socket_bind(socket, address);
    if (!res)
    {
        socket_close(socket);
        return DataConnectionError(res);
    }

    res = socket_listen(socket, DataConnection::max_clients);
    if (!res)
    {
        socket_close(socket);
        return DataConnectionError(res);
    }

    auto* server = BEE_NEW(system_allocator(), DataConnection);
    server->address = BEE_MOVE(address);
    server->socket = socket;
    server->flags = DataConnectionFlags::server | DataConnectionFlags::connected;
    server->thread_data.resize(job_system_worker_count());
    return server;
}

Result<DataConnection*, DataConnectionError> create_client()
{
    auto* client = BEE_NEW(system_allocator(), DataConnection);
    client->flags = DataConnectionFlags::client;
    client->thread_data.resize(job_system_worker_count());
    return client;
}

Result<void, DataConnectionError> destroy_connection(DataConnection* connection)
{
    const auto connected_client_flag = DataConnectionFlags::client | DataConnectionFlags::connected;
    if ((connection->flags & connected_client_flag) == connected_client_flag)
    {
        auto res = socket_shutdown(connection->socket);
        if (!res)
        {
            return DataConnectionError(res);
        }
    }

    const auto result = socket_close(connection->socket);
    BEE_ASSERT_F(result, "Failed to destroy DataConnection server: %s", result.unwrap_error().to_string());
    BEE_DELETE(system_allocator(), connection);
    return {};
}

DataConnectionFlags get_flags(const DataConnection* connection)
{
    return connection->flags;
}

Result<void, DataConnectionError> connect_client(DataConnection* client, const SocketAddressFamily address_family, const char* hostname, const port_t port)
{
    if ((client->flags & DataConnectionFlags::client) == DataConnectionFlags::invalid)
    {
        return DataConnectionError(DataConnectionError::invalid_client);
    }

    if ((client->flags & DataConnectionFlags::connected) != DataConnectionFlags::invalid)
    {
        return DataConnectionError(DataConnectionError::connected);
    }

    SocketAddress address{};
    auto res = socket_reset_address(&address, SocketType::tcp, address_family, hostname, port);
    if (!res)
    {
        return DataConnectionError(res);
    }

    socket_t socket{};
    res = socket_open(&socket, address);
    if (!res)
    {
        return DataConnectionError(res);
    }

    res = socket_connect(&socket, address);
    if (!res)
    {
        socket_close(socket);
        return DataConnectionError(res);
    }

    client->flags |= DataConnectionFlags::connected;
    return {};
}

Result<void, DataConnectionError> disconnect_client(DataConnection* client)
{
    if ((client->flags & DataConnectionFlags::client) == DataConnectionFlags::invalid)
    {
        return DataConnectionError(DataConnectionError::invalid_client);
    }

    if ((client->flags & DataConnectionFlags::connected) == DataConnectionFlags::invalid)
    {
        return DataConnectionError(DataConnectionError::disconnected);
    }

    auto res = socket_shutdown(client->socket);
    if (!res)
    {
        return DataConnectionError(res);
    }

    client->flags &= ~DataConnectionFlags::connected;
    return {};
}

Result<void, DataConnectionError> send_packet(DataConnection* connection, const Type type, const i32 serialized_size, const void* serialized_data)
{
    if ((connection->flags & DataConnectionFlags::connected) == DataConnectionFlags::invalid)
    {
        return DataConnectionError(DataConnectionError::disconnected);
    }

    auto& thread = connection->get_thread();

    DataConnectionPacket msg{};
    msg.type_hash = type->hash;
    msg.serialized_size = serialized_size;

    thread.send_buffer.append({ reinterpret_cast<const u8*>(&msg), sizeof(DataConnectionPacket) });
    thread.send_buffer.append({ static_cast<const u8*>(serialized_data), msg.serialized_size });

    return {};
}

Result<Allocator*, DataConnectionError> get_packet_allocator(DataConnection* connection)
{
    if ((connection->flags & DataConnectionFlags::connected) == DataConnectionFlags::invalid)
    {
        return DataConnectionError(DataConnectionError::disconnected);
    }

    return &connection->get_thread().packet_allocator;
}

static Result<void, DataConnectionError> recv_socket(socket_t socket, DynamicArray<DataConnectionPacket>* packets, DynamicArray<u8>* data)
{
    DataConnectionPacket header{};

    // Recv as many serialized packets as we can
    while (true)
    {
        packets->emplace_back();

        // get the header and check that it's valid
        int recv_count = socket_recv(socket, &packets->back(), sizeof(DataConnectionPacket));
        if (recv_count == 0)
        {
            packets->pop_back();
            return {};
        }

        if (recv_count != sizeof(DataConnectionPacket))
        {
            packets->pop_back();
            return { DataConnectionError::packet_failed };
        }

        // Reserve enough bytes to contain the serialized data
        auto& packet = packets->back();
        packet.offset = data->size();
        data->append(header.serialized_size, 0);

        // get the serialized data and check that the bytecount received is valid
        recv_count = socket_recv(socket, data->data() + packet.offset, header.serialized_size);

        if (recv_count != header.serialized_size)
        {
            // erase the newly received data if recv failed
            data->resize(packet.offset);
            packets->pop_back();
            return { DataConnectionError::packet_failed };
        }
    }

    // success
    return {};
}

static Result<void, DataConnectionError> send_connection(DataConnection* connection)
{
    for (auto& thread : connection->thread_data)
    {
        while (thread.flush_offset < thread.send_buffer.size())
        {
            auto* packet = thread.send_buffer.data() + thread.flush_offset;
            auto* header = reinterpret_cast<DataConnectionPacket*>(packet);
            const i32 packet_size = sizeof(DataConnectionPacket) + header->serialized_size;

            auto res = socket_send(connection->socket, packet, packet_size);
            if (!res)
            {
                return DataConnectionError(res.unwrap_error());
            }

            thread.flush_offset += packet_size;
        }

        thread.flush_offset = 0;
        thread.send_buffer.clear();
        thread.packet_allocator.reset();
    }

    return {};
}

static Result<void, DataConnectionError> flush_server(DataConnection* connection, const u64 timeout_ms)
{
    socket_fd_zero(&connection->read_set);
    socket_fd_set(connection->socket, &connection->read_set);

    // Add all the active client connections to the read set
    for (const auto& client : connection->clients)
    {
        if (client.in_use)
        {
            socket_fd_set(client.socket, &connection->read_set);
        }
    }

    // timeout on the select to make sure we don't get indefinitely stuck waiting for a connection
    timeval timeout{};
    timeout.tv_sec = static_cast<long>(timeout_ms / 1000);
    timeout.tv_usec = static_cast<long>(timeout_ms % 1000) * 1000;

    // Call select to get read ops
    auto res = socket_select(connection->socket, &connection->read_set, nullptr, nullptr, timeout);
    if (!res)
    {
        return DataConnectionError(res);
    }

    // Check if the server has new connections and connect a new client socket if one is available
    const int client_count = res.unwrap();
    const bool has_new_connections = client_count > 0 && socket_fd_isset(connection->socket, &connection->read_set);
    if (has_new_connections)
    {
        // Find a spare client slot otherwise return error
        const int new_client_index = find_index_if(connection->clients, [&](const Client& client)
        {
            return !client.in_use;
        });

        if (new_client_index < 0)
        {
            return DataConnectionError(DataConnectionError::max_clients);
        }

        // Found a spare client so we can try and accept the connection
        auto& client = connection->clients[new_client_index];

        // Try and accept the new client connection
        auto accept_res = socket_accept(connection->socket, &client.socket);
        if (!accept_res)
        {
            return DataConnectionError(accept_res);
        }

        client.in_use = true; // success
    }

    connection->clear_recv_buffers();

    // Recv all data to the server first before sending new pending messages
    for (auto& client : connection->clients)
    {
        if (!client.in_use || !socket_fd_isset(client.socket, &connection->read_set))
        {
            // client is either not in use or hasn't sent enough data to be read
            continue;
        }

        auto recv_result = recv_socket(client.socket, &connection->recv_packets, &connection->recv_buffer);
        if (!recv_result)
        {
            return recv_result.unwrap_error();
        }
    }

    // Send all the pending queued packets
    return send_connection(connection);
}

static Result<void, DataConnectionError> flush_client(DataConnection* connection)
{
    // Receive new packets and then send the pending ones
    connection->clear_recv_buffers();
    auto res = recv_socket(connection->socket, &connection->recv_packets, &connection->recv_buffer);
    if (!res)
    {
        return res;
    }
    // send pending packets from the client
    return send_connection(connection);
}

Result<void, DataConnectionError> flush(DataConnection* connection, const u64 timeout_ms)
{
    if ((connection->flags & DataConnectionFlags::connected) == DataConnectionFlags::invalid)
    {
        return DataConnectionError(DataConnectionError::disconnected);
    }

    if ((connection->flags & DataConnectionFlags::server) != DataConnectionFlags::invalid)
    {
        return flush_server(connection, timeout_ms);
    }

    return flush_client(connection);
}

i32 get_received_data(DataConnection* connection, const DataConnectionPacket** packets, const void** data)
{
    if (connection->recv_packets.empty())
    {
        return 0;
    }

    if (packets != nullptr)
    {
        *packets = connection->recv_packets.data();
    }
    if (data != nullptr)
    {
        *data = connection->recv_buffer.data();
    }

    return connection->recv_packets.size();
}


} // namespace bee


static bee::DataConnectionModule g_module{};

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    g_module.startup = bee::startup;
    g_module.shutdown = bee::shutdown;
    g_module.create_server = bee::create_server;
    g_module.create_client = bee::create_client;
    g_module.destroy_connection = bee::destroy_connection;
    g_module.get_flags = bee::get_flags;
    g_module.connect_client = bee::connect_client;
    g_module.disconnect_client = bee::disconnect_client;
    g_module.send_packet = bee::send_packet;
    g_module.get_packet_allocator = bee::get_packet_allocator;
    g_module.flush = bee::flush;
    g_module.get_received_data = bee::get_received_data;
    loader->set_module(BEE_DATA_CONNECTION_MODULE_NAME, &g_module, state);
}

BEE_PLUGIN_VERSION(0, 0, 0)