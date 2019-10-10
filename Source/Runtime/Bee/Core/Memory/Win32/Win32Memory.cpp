/*
 *  Win32Memory.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Memory/Memory.hpp"
#include "Bee/Core/Win32/MinWindows.h"

namespace bee {


const char* get_last_error(char* dst_buffer, const i32 buffer_size)
{
    const auto error_code = GetLastError();
    const auto formatting_options = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;

    FormatMessage(
        formatting_options,                         // allocate a buffer large enough for the message and lookup system messages
        nullptr,                                    // source of the message - null as we're looking up system messages
        error_code,                                 // message ID for the requested message
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),  // default language
        dst_buffer,                                 // destination for the message buffer
        static_cast<DWORD>(buffer_size),            // buffer size
        nullptr                                     // no arguments
    );

    return dst_buffer;
}

const char* get_last_error()
{
    static constexpr int message_buffer_size = 1024;
    static char message_buffer[message_buffer_size]{0};
    return get_last_error(message_buffer, message_buffer_size);
}


size_t get_page_size() noexcept
{
    _SYSTEM_INFO system_info{};
    GetSystemInfo(&system_info);
    return static_cast<size_t>(system_info.dwPageSize);
}

size_t get_min_stack_size() noexcept
{
    ULONG_PTR low_limit = 0;
    ULONG_PTR high_limit = 0;
    GetCurrentThreadStackLimits(&low_limit, &high_limit);
    return static_cast<size_t>(low_limit);
}

size_t get_max_stack_size() noexcept
{
    ULONG_PTR low_limit = 0;
    ULONG_PTR high_limit = 0;
    GetCurrentThreadStackLimits(&low_limit, &high_limit);
    return static_cast<size_t>(high_limit);
}

size_t get_canonical_stack_size() noexcept
{
    // The default stack size set by the MSVC linker is 1MB
    // see: https://docs.microsoft.com/en-us/windows/desktop/procthread/thread-stack-size
    return mebibytes(1);
}

bool guard_memory(void* memory, size_t num_bytes, MemoryProtectionMode protection) noexcept
{
    const auto is_write = (protection & MemoryProtectionMode::write) != MemoryProtectionMode::none;
    const auto is_read = (protection & MemoryProtectionMode::read) != MemoryProtectionMode::none;
    DWORD new_protect;

    if ((protection & MemoryProtectionMode::exec) != MemoryProtectionMode::none)
    {
        new_protect = is_write ? PAGE_EXECUTE_READWRITE : is_read ? PAGE_EXECUTE_READ : PAGE_EXECUTE;
    }
    else
    {
        new_protect = is_write ? PAGE_READWRITE : PAGE_READONLY;
    }

    DWORD old_protect = 0;
    const auto protect_success = VirtualProtect(memory, num_bytes, new_protect, &old_protect);
    if (BEE_FAIL_F(protect_success, "Failed to guard virtual memory address %p: %s", memory, get_last_error()))
    {
        return false;
    }

    return true;
}


} // namespace bee