/*
 *  Socket.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"
#include "Bee/Core/NumericTypes.hpp"

#if BEE_OS_WINDOWS == 1
    #include "Bee/Core/Win32/MinWindows.h"

    #include <WinSock2.h>
    #include <WS2tcpip.h>
#else
    #error Unsupported platform
#endif // BEE_OS_WINDOWS == 1


namespace bee {


#define BEE_IPV4_LOCALHOST "127.0.0.1"


enum class SocketType
{
    tcp,
    udp
};

enum class SocketAddressFamily
{
    ipv4,
    ipv6
};

#define BEE_SOCKET_SUCCESS 0

#if BEE_OS_WINDOWS == 1

    using socket_t = SOCKET;

    using fd_set_t = fd_set;

    using addrinfo_t = addrinfo;

#else
    #error Platform not supported
#endif // BEE_OS_WINDOWS == 1

using port_t = u16;


enum class SocketError : i32
{
    success = 0,
    api_not_initialized,
    network_failure,
    bad_address,
    socket_not_connected,
    function_call_interrupted,
    blocking_operation_executing,
    nonsocket_operation_detected,
    operation_not_supported,
    send_after_socket_shutdown,
    resource_temporarily_unavailable,
    message_too_long,
    invalid_argument,
    connection_aborted_by_host,
    connection_timed_out,
    connection_reset_by_peer,
    unknown_error
};


struct BEE_CORE_API SocketAddress
{
    addrinfo_t* info { nullptr };

    ~SocketAddress();

    inline const addrinfo_t* operator->() const
    {
        return info;
    }

    inline addrinfo_t* operator->()
    {
        return info;
    }

    const char* to_string() const;
};



BEE_CORE_API i32 socket_reset_address(SocketAddress* address, const SocketType type, const SocketAddressFamily address_family, const char* hostname, const port_t port);

BEE_CORE_API i32 socket_startup();

BEE_CORE_API const char* socket_code_to_string(const i32 code);

BEE_CORE_API SocketError socket_code_to_error(const i32 code);

BEE_CORE_API i32 socket_cleanup();

BEE_CORE_API i32 socket_open(socket_t* dst, const SocketAddress& address);

BEE_CORE_API i32 socket_close(const socket_t& socket);

BEE_CORE_API i32 socket_connect(socket_t* dst, const SocketAddress& address);

BEE_CORE_API i32 socket_shutdown(const socket_t client);

BEE_CORE_API i32 socket_bind(const socket_t& socket, const SocketAddress& address);

BEE_CORE_API i32 socket_listen(const socket_t& socket, i32 max_waiting_clients);

BEE_CORE_API i32 socket_accept(const socket_t& socket, socket_t* client);


// close connection if result == 0, otherwise success with bytes recieved if > 0, otherwise error
BEE_CORE_API i32 socket_recv(const socket_t& client, char* buffer, const i32 buffer_length);

BEE_CORE_API i32 socket_send(const socket_t& client, const char* buffer, const i32 buffer_length);

BEE_CORE_API i32 socket_select(const socket_t& socket, fd_set_t* read_fd_set, fd_set_t* write_fd_set, fd_set_t* except_fd_set, const timeval& timeout);

BEE_CORE_API void socket_fd_zero(fd_set_t* set);

BEE_CORE_API void socket_fd_set(const socket_t& socket, fd_set_t* set);

BEE_CORE_API i32 socket_fd_isset(const socket_t& socket, fd_set_t* read_set);


} // namespace bee