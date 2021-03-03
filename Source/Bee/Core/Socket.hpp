/*
 *  Socket.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Result.hpp"
#include "Bee/Core/Noncopyable.hpp"


#if BEE_OS_WINDOWS == 1
    #include "Bee/Core/Win32/MinWindows.h"

    BEE_PUSH_WARNING
        BEE_DISABLE_PADDING_WARNINGS
        #include <WinSock2.h>
        #include <WS2tcpip.h>
    BEE_POP_WARNING
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


enum class SocketStatus : i32
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

struct BEE_CORE_API SocketError
{
    i32 code { BEE_SOCKET_SUCCESS };

    const char* to_string() const;

    SocketStatus to_status() const;

    inline operator bool() const
    {
        return code == BEE_SOCKET_SUCCESS;
    }
};


struct BEE_CORE_API SocketAddress : public Noncopyable
{
    addrinfo_t* info { nullptr };

    SocketAddress() = default;

    SocketAddress(SocketAddress&& other) noexcept
        : info(other.info)
    {
        other.info = nullptr;
    }

    ~SocketAddress();

    SocketAddress& operator=(SocketAddress&& other) noexcept
    {
        info = other.info;
        other.info = nullptr;
        return *this;
    }

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



BEE_CORE_API Result<void, SocketError> socket_reset_address(SocketAddress* address, const SocketType type, const SocketAddressFamily address_family, const char* hostname, const port_t port);

BEE_CORE_API Result<void, SocketError> socket_startup();

BEE_CORE_API Result<void, SocketError> socket_cleanup();

BEE_CORE_API Result<void, SocketError> socket_open(socket_t* dst, const SocketAddress& address);

BEE_CORE_API Result<void, SocketError> socket_close(const socket_t& socket);

BEE_CORE_API Result<void, SocketError> socket_connect(socket_t* dst, const SocketAddress& address);

BEE_CORE_API Result<void, SocketError> socket_shutdown(const socket_t socket);

BEE_CORE_API Result<void, SocketError> socket_bind(const socket_t& socket, const SocketAddress& address);

BEE_CORE_API Result<void, SocketError> socket_listen(const socket_t& socket, i32 max_waiting_clients);

BEE_CORE_API Result<void, SocketError> socket_accept(const socket_t& socket, socket_t* client);


// close connection if result == 0, otherwise success with bytes recieved if > 0, otherwise error
BEE_CORE_API Result<i32, SocketError> socket_recv(const socket_t& socket, void* buffer, const i32 buffer_length);

BEE_CORE_API Result<i32, SocketError> socket_send(const socket_t& socket, const void* buffer, const i32 buffer_length);

BEE_CORE_API Result<i32, SocketError> socket_select(const socket_t& socket, fd_set_t* read_fd_set, fd_set_t* write_fd_set, fd_set_t* except_fd_set, const timeval& timeout);

BEE_CORE_API void socket_fd_zero(fd_set_t* set);

BEE_CORE_API void socket_fd_set(const socket_t& socket, fd_set_t* set);

BEE_CORE_API bool socket_fd_isset(const socket_t& socket, fd_set_t* read_set);


} // namespace bee