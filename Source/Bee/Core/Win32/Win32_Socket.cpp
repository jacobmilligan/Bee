/*
 *  Win32Socket.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Socket.hpp"
#include "Bee/Core/String.hpp"


namespace bee {


const char* SocketError::to_string() const
{
    static thread_local StaticString<1024> msg { 1024, '\0' };

    if (code == BEE_SOCKET_SUCCESS)
    {
        return "Success";
    }

    return win32_format_error(code, msg.data(), msg.size());
}

SocketStatus SocketError::to_status() const
{
#define BEE_SOCKET_CODE_MAPPING(wsa_code, bee_error) case wsa_code: return SocketStatus::bee_error;
    switch (code == SOCKET_ERROR ? WSAGetLastError() : code)
    {
        case 0: return SocketStatus::success;
        BEE_SOCKET_CODE_MAPPING(WSANOTINITIALISED, api_not_initialized)
        BEE_SOCKET_CODE_MAPPING(WSAENETDOWN, network_failure)
        BEE_SOCKET_CODE_MAPPING(WSAEFAULT, bad_address)
        BEE_SOCKET_CODE_MAPPING(WSAENOTCONN, socket_not_connected)
        BEE_SOCKET_CODE_MAPPING(WSAEINTR, function_call_interrupted)
        BEE_SOCKET_CODE_MAPPING(WSAEINPROGRESS, blocking_operation_executing)
        BEE_SOCKET_CODE_MAPPING(WSAENOTSOCK, nonsocket_operation_detected)
        BEE_SOCKET_CODE_MAPPING(WSAEOPNOTSUPP, operation_not_supported)
        BEE_SOCKET_CODE_MAPPING(WSAESHUTDOWN, send_after_socket_shutdown)
        BEE_SOCKET_CODE_MAPPING(WSAEWOULDBLOCK, resource_temporarily_unavailable)
        BEE_SOCKET_CODE_MAPPING(WSAEMSGSIZE, message_too_long)
        BEE_SOCKET_CODE_MAPPING(WSAEINVAL, invalid_argument)
        BEE_SOCKET_CODE_MAPPING(WSAECONNABORTED, connection_aborted_by_host)
        BEE_SOCKET_CODE_MAPPING(WSAETIMEDOUT, connection_timed_out)
        BEE_SOCKET_CODE_MAPPING(WSAECONNRESET, connection_reset_by_peer)
        default: break;
    }
#undef BEE_SOCKET_CODE_MAPPING

    return SocketStatus::unknown_error;
}

SocketAddress::~SocketAddress()
{
    if (info != nullptr)
    {
        freeaddrinfo(info);
    }
    info = nullptr;
}

const char* SocketAddress::to_string() const
{
    return info->ai_canonname;
}

static Result<void, SocketError> void_or_err(const i32 code)
{
    if (code != 0)
    {
        return SocketError { code };
    }
    return {};
}

Result<void, SocketError> socket_reset_address(SocketAddress* address, const SocketType type, const SocketAddressFamily address_family, const char* hostname, const port_t port)
{
    if (address->info != nullptr)
    {
        address->~SocketAddress();
    }

    ADDRINFOA hints{};
    hints.ai_family = address_family == SocketAddressFamily::ipv4 ? AF_INET : AF_INET6;
    hints.ai_socktype = type == SocketType::tcp ? SOCK_STREAM : SOCK_DGRAM;
    hints.ai_protocol = type == SocketType::tcp ? IPPROTO_TCP : IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE | AI_CANONNAME;

    char port_string[8];
    str::format_buffer(port_string, 8, "%u", htons(port));

    return void_or_err(getaddrinfo(hostname, port_string, &hints, &address->info));
}

Result<void, SocketError> socket_startup()
{
    WSADATA wsa;
    return void_or_err(WSAStartup(MAKEWORD(2, 2), &wsa));
}

Result<void, SocketError> socket_cleanup()
{
    return void_or_err(WSACleanup());
}

Result<void, SocketError> socket_open(socket_t* dst, const SocketAddress& address)
{
    *dst = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (*dst == INVALID_SOCKET)
    {
        return SocketError { WSAGetLastError() };
    }

    return {};
}

Result<void, SocketError> socket_close(const socket_t& socket)
{
    return void_or_err(closesocket(socket));
}

Result<void, SocketError> socket_connect(socket_t* dst, const SocketAddress& address)
{
    auto result = connect(*dst, address->ai_addr, (int)address->ai_addrlen);

    if (result == 0)
    {
        return {};
    }

    result = WSAGetLastError();

    socket_close(*dst);
    *dst = INVALID_SOCKET;
    return SocketError { result };
}

Result<void, SocketError> socket_shutdown(const socket_t socket)
{
    auto result = shutdown(socket, SD_SEND);
    if (result == 0)
    {
        return {};
    }
    result = WSAGetLastError();

    socket_close(socket);
    return SocketError { result };
}

Result<void, SocketError> socket_bind(const socket_t& socket, const SocketAddress& address)
{
    auto result = bind(socket, address->ai_addr, sign_cast<i32>(address->ai_addrlen));
    if (result == 0)
    {
        return {};
    }

    result = WSAGetLastError();
    socket_close(socket);
    return SocketError { result };
}

Result<void, SocketError> socket_listen(const socket_t& socket, const i32 max_waiting_clients)
{
    auto result = listen(socket, max_waiting_clients);
    if (result == 0)
    {
        return {};
    }

    result = WSAGetLastError();
    socket_close(socket);
    return SocketError { result };
}

Result<void, SocketError> socket_accept(const socket_t& socket, socket_t* client)
{
    *client = accept(socket, nullptr, nullptr);
    if (*client != INVALID_SOCKET)
    {
        return {};
    }
    return SocketError { WSAGetLastError() };
}


// close connection if result == 0, otherwise success with bytes recieved if > 0, otherwise error
Result<i32, SocketError> socket_recv(const socket_t& socket, void* buffer, const i32 buffer_length)
{
    const int result = recv(socket, static_cast<char*>(buffer), buffer_length, 0);

    if (result == SOCKET_ERROR)
    {
        return SocketError { result };
    }

    return result;
}

Result<i32, SocketError> socket_send(const socket_t& socket, const void* buffer, const i32 buffer_length)
{
    const int result = send(socket, static_cast<const char*>(buffer), buffer_length, 0);

    if (result == SOCKET_ERROR)
    {
        return SocketError { result };
    }

    return result;
}

Result<i32, SocketError> socket_select(const socket_t& socket, fd_set_t* read_fd_set, fd_set_t* write_fd_set, fd_set_t* except_fd_set, const timeval& timeout)
{
    const int result = select(0, read_fd_set, write_fd_set, except_fd_set, &timeout);

    if (result == SOCKET_ERROR)
    {
        return SocketError { result };
    }

    return result;
}

void socket_fd_zero(fd_set_t* set)
{
    FD_ZERO(set);
}

void socket_fd_set(const socket_t& socket, fd_set_t* set)
{
    FD_SET(socket, set);
}

bool socket_fd_isset(const socket_t& socket, fd_set_t* read_set)
{
    return FD_ISSET(socket, read_set) != 0;
}


} // namespace bee