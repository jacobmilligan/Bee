/*
 *  Win32Socket.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Socket.hpp"
#include "Bee/Core/String.hpp"


namespace bee {


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

i32 socket_reset_address(SocketAddress* address, const SocketType type, const SocketAddressFamily address_family, const char* hostname, const port_t port)
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
    str::format(port_string, 8, "%u", htons(port));

    return getaddrinfo(hostname, port_string, &hints, &address->info);
}

i32 socket_startup()
{
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa);
}

i32 socket_cleanup()
{
    return WSACleanup();
}

const char* socket_code_to_string(const i32 code)
{
    static thread_local char msg_buffer[1024];
    return win32_format_error(code == SOCKET_ERROR ? WSAGetLastError() : code, msg_buffer, static_array_length(msg_buffer));
}

SocketError socket_code_to_error(const i32 code)
{
#define BEE_SOCKET_CODE_MAPPING(wsa_code, bee_error) case wsa_code: return SocketError::bee_error;
    switch (code == SOCKET_ERROR ? WSAGetLastError() : code)
    {
        case 0: return SocketError::success;
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

    return SocketError::unknown_error;
}

i32 socket_open(socket_t* dst, const SocketAddress& address)
{
    *dst = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (*dst == INVALID_SOCKET)
    {
        return WSAGetLastError();
    }

    return BEE_SOCKET_SUCCESS;
}

i32 socket_close(const socket_t& socket)
{
    return closesocket(socket);
}

i32 socket_connect(socket_t* dst, const SocketAddress& address)
{
    auto result = connect(*dst, address->ai_addr, (int)address->ai_addrlen);

    if (result == 0)
    {
        return BEE_SOCKET_SUCCESS;
    }
    result = WSAGetLastError();

    socket_close(*dst);
    *dst = INVALID_SOCKET;
    return result;
}

i32 socket_shutdown(const socket_t client)
{
    auto result = shutdown(client, SD_SEND);
    if (result == 0)
    {
        return BEE_SOCKET_SUCCESS;
    }
    result = WSAGetLastError();

    socket_close(client);
    return result;
}

i32 socket_bind(const socket_t& socket, const SocketAddress& address)
{
    auto result = bind(socket, address->ai_addr, sign_cast<i32>(address->ai_addrlen));
    if (result == 0)
    {
        return BEE_SOCKET_SUCCESS;
    }

    result = WSAGetLastError();
    socket_close(socket);
    return result;
}

i32 socket_listen(const socket_t& socket, const i32 max_waiting_clients)
{
    auto result = listen(socket, max_waiting_clients);
    if (result == 0)
    {
        return BEE_SOCKET_SUCCESS;
    }

    result = WSAGetLastError();
    socket_close(socket);
    return result;
}

i32 socket_accept(const socket_t& socket, socket_t* client)
{
    *client = accept(socket, nullptr, nullptr);
    if (*client != INVALID_SOCKET)
    {
        return BEE_SOCKET_SUCCESS;
    }
    return WSAGetLastError();
}


// close connection if result == 0, otherwise success with bytes recieved if > 0, otherwise error
i32 socket_recv(const socket_t& client, char* buffer, const i32 buffer_length)
{
    return recv(client, buffer, buffer_length, 0);
}

i32 socket_send(const socket_t& client, const char* buffer, const i32 buffer_length)
{
    return send(client, buffer, buffer_length, 0);
}

i32 socket_select(const socket_t& socket, fd_set_t* read_fd_set, fd_set_t* write_fd_set, fd_set_t* except_fd_set, const timeval& timeout)
{
    return select(0, read_fd_set, write_fd_set, except_fd_set, &timeout);
}

void socket_fd_zero(fd_set_t* set)
{
    FD_ZERO(set);
}

void socket_fd_set(const socket_t& socket, fd_set_t* set)
{
    FD_SET(socket, set);
}

i32 socket_fd_isset(const socket_t& socket, fd_set_t* read_set)
{
    return FD_ISSET(socket, read_set);
}


} // namespace bee