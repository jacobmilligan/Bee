/*
 *  Server.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Socket.hpp"
#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Containers/Array.hpp"
#include "Bee/AssetCompiler/Messages.hpp"
#include "Bee/Core/Serialization/MemorySerializer.hpp"


namespace bee {


#define BEE_AC_DEFAULT_PORT 8888

#ifndef BEE_AC_MAX_CLIENTS
    #define BEE_AC_MAX_CLIENTS 16
#endif // BEE_AC_MAX_CLIENTS

#ifndef BEE_AC_MAX_PENDING_JOBS_PER_CLIENT
    #define BEE_AC_MAX_PENDING_JOBS_PER_CLIENT 32
#endif // BEE_AC_MAX_PENDING_JOBS_PER_CLIENT


enum class ACConnectionType
{
    not_connected,
    server,
    client
};


struct AssetCompilerConnection
{
    ACConnectionType    connection_type { ACConnectionType::not_connected };
    socket_t            socket;
    SocketAddress       current_address;
    Allocator*          message_allocator { nullptr };
};


/*
 ********************************************
 *
 * # Asset Compiler API
 *
 ********************************************
 */
BEE_ASSETCOMPILER_API bool asset_compiler_listen(const SocketAddress& address, AssetCompilerConnection* connection, Allocator* message_allocator = system_allocator());

BEE_ASSETCOMPILER_API bool asset_compiler_connect(const SocketAddress& address, AssetCompilerConnection* connection, Allocator* message_allocator = system_allocator());

BEE_ASSETCOMPILER_API bool asset_compiler_wait_last_message(const AssetCompilerConnection& connection);

template <typename MsgType>
bool asset_compiler_send_message(const AssetCompilerConnection& client, const MsgType& msg)
{
    static_assert(std::is_base_of_v<ACMessage, MsgType>, "Bee Asset Compiler: MsgType must derive from ACMessage");
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



} // namespace bee