/*
 *  Win32Socket.cpp
 *  Skyrocket
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


const char* wsa_get_last_error_string()
{
    static char msg_buffer[1024];
    return win32_format_error(WSAGetLastError(), msg_buffer, 1024);
}

bool socket_reset_address(SocketAddress* address, const SocketType type, const SocketAddressFamily address_family, const char* hostname, const u16 port)
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

    const auto iresult = getaddrinfo(hostname, port_string, &hints, &address->info);
    if (BEE_FAIL_F(iresult == 0, "Failed to get address info: %d", iresult))
    {
        return false;
    }

    return true;
}

bool socket_startup()
{
    WSADATA wsa;
    const auto startup_result = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (BEE_FAIL_F(startup_result == 0, "Failed to initialize winsock library: %s", wsa_get_last_error_string()))
    {
        return false;
    }
    return true;
}

void socket_cleanup()
{
    WSACleanup();
}

bool socket_open(socket_t* dst, const SocketAddress& address)
{
    *dst = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    return BEE_CHECK_F(*dst != INVALID_SOCKET, "Failed to open socket connection");
}

void socket_close(const socket_t& socket)
{
    closesocket(socket);
}

void socket_bind(const socket_t& socket, const SocketAddress& address)
{
    const auto result = bind(socket, address->ai_addr, sign_cast<i32>(address->ai_addrlen));
    if (BEE_FAIL_F(result != SOCKET_ERROR, "Failed to bind socket: %s", wsa_get_last_error_string()))
    {
        socket_close(socket);
    }
}

void socket_listen(const socket_t& socket)
{
    const auto result = listen(socket, 3);
    if (BEE_FAIL_F(result != SOCKET_ERROR, "Failed to listen to socket: %s", wsa_get_last_error_string()))
    {
        socket_close(socket);
    }
}

bool socket_accept(const socket_t& socket, socket_t* client)
{
    *client = accept(socket, nullptr, nullptr);
    if (BEE_FAIL_F(*client >= 0, "Failed to accept a socket connection: %s", wsa_get_last_error_string()))
    {
        return false;
    }
    return *client != INVALID_SOCKET;
}


// close connection if result == 0, otherwise success with bytes recieved if > 0, otherwise error
i32 socket_recv(const socket_t& client, char* buffer, const i32 buffer_length)
{
    const auto result = recv(client, buffer, buffer_length, 0);
    BEE_ASSERT_F(result >= 0, "Failed to recv socket data: %s", wsa_get_last_error_string());
    return result;
}

i32 socket_send(const socket_t& server, const char* buffer, const i32 buffer_length)
{
    const auto result = send(server, buffer, buffer_length, 0);
    BEE_ASSERT_F(result >= 0, "Failed to send socket data: %s", wsa_get_last_error_string());
    return result;
}

i32 socket_select(const socket_t& socket, fd_set_t* read_fd_set, fd_set_t* write_fd_set, fd_set_t* except_fd_set, const timeval& timeout)
{
    const auto result = select(0, read_fd_set, write_fd_set, except_fd_set, &timeout);
    BEE_ASSERT_F(result >= 0, "Failed to socket select: %s", wsa_get_last_error_string());
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

i32 socket_fd_isset(const socket_t& socket, fd_set_t* read_set)
{
    return FD_ISSET(socket, read_set);
}


} // namespace bee