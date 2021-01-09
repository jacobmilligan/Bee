/*
 *  MessageServer.hpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Socket.hpp"
#include "Bee/Core/Enum.hpp"
#include "Bee/Core/Move.hpp"
#include "Bee/Core/Serialization/BinarySerializer.hpp"


namespace bee {


BEE_FLAGS(DataConnectionFlags, u32)
{
    invalid         = 0u,
    connected       = 1u << 0u,
    client          = 1u << 1u,
    server          = 1u << 2u
};

struct DataConnectionError
{
    enum Enum
    {
        invalid_client,
        invalid_server,
        connected,
        disconnected,
        max_clients,
        packet_failed,
        count,
        socket_error,
        socket_error_last = 0x7FFFFFFF,
    };

    BEE_ENUM_STRUCT(DataConnectionError);

    template <typename T>
    DataConnectionError(const Result<T, SocketError>& res)
        : value(static_cast<Enum>(Enum::socket_error + res.unwrap_error().code))
    {}

    const char* to_string() const
    {
        if (value >= Enum::socket_error)
        {
            return SocketError{ value - Enum::socket_error }.to_string();
        }

        BEE_TRANSLATION_TABLE(value, Enum, const char*, Enum::count,
            "DataConnection is not a client connection",
            "DataConnection is not a server connection",
            "DataConnection is already connected",
            "DataConnection is not connected",
            "Max pending client connections reached on server connection",
            "Data packet format was invalid or missing a header"
        );
    }
};

struct DataConnectionPacket
{
    u32 type_hash;
    i32 offset { 0 };
    i32 serialized_size { 0 };
};

#define BEE_DATA_CONNECTION_MODULE_NAME "BEE_DATA_CONNECTION"

struct DataConnection;
struct DataConnectionModule
{
    Result<void, DataConnectionError> (*startup)() { nullptr };

    Result<void, DataConnectionError> (*shutdown)() { nullptr };

    Result<DataConnection*, DataConnectionError> (*create_server)(const SocketAddressFamily address_family, const char* hostname, const port_t port) { nullptr };

    Result<DataConnection*, DataConnectionError> (*create_client)() { nullptr };

    Result<void, DataConnectionError> (*destroy_connection)(DataConnection* connection) { nullptr };

    DataConnectionFlags (*get_flags)(const DataConnection* connection) { nullptr };

    Result<void, DataConnectionError> (*connect_client)(DataConnection* client, const SocketAddressFamily address_family, const char* hostname, const port_t port) { nullptr };

    Result<void, DataConnectionError> (*disconnect_client)(DataConnection* client) { nullptr };

    Result<void, DataConnectionError> (*send_packet)(DataConnection* connection, const Type type, const i32 serialized_size, const void* serialized_data) { nullptr };

    Result<Allocator*, DataConnectionError> (*get_packet_allocator)(DataConnection* connection) { nullptr };

    Result<void, DataConnectionError> (*flush)(DataConnection* connection, const u64 timeout_ms) { nullptr };

    i32 (*get_received_data)(DataConnection* connection, const DataConnectionPacket** packets, const void** data) { nullptr };

    template <typename T>
    inline Result<void, DataConnectionError> send(DataConnection* connection, T* msg)
    {
        auto allocator = get_packet_allocator(connection);
        if (!allocator)
        {
            return allocator.unwrap_error();
        }

        DynamicArray<u8> buffer(allocator.unwrap());
        BinarySerializer serializer(&buffer);
        serialize(SerializerMode::writing, &serializer, msg);

        return send_packet(connection, get_type<T>(), buffer.size(), buffer.data());
    }
};


} // namespace bee



