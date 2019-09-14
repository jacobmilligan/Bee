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


namespace bee {


#define BEE_AC_DEFAULT_PORT 8888

#ifndef BEE_AC_MAX_CLIENTS
    #define BEE_AC_MAX_CLIENTS 16
#endif // BEE_AC_MAX_CLIENTS


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

BEE_ASSETCOMPILER_API bool asset_compiler_shutdown_server(const AssetCompilerConnection& client);


} // namespace bee