/*
 *  Win32VMAllocator.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Win32/MinWindows.h"

#include <memoryapi.h>
#include <errhandlingapi.h>

namespace bee {


void* vm_map(const size_t size)
{
    auto* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    BEE_ASSERT_F(ptr != nullptr, "Failed to map virtual memory: Win32 error code: %s", win32_get_last_error_string());
    return ptr;
}

void vm_unmap(void* ptr, const size_t size)
{
    const auto success = VirtualFree(ptr, 0, MEM_RELEASE);
    BEE_ASSERT_F(success, "Failed to unmap virtual memory: Win32 error code: %s", win32_get_last_error_string());
}

void* vm_reserve(const size_t size)
{
    auto* ptr = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
    BEE_ASSERT_F(ptr != nullptr, "Failed to reserve virtual memory: Win32 error code: %s", win32_get_last_error_string());
    return ptr;
}

void vm_commit(void* ptr, const size_t size)
{
    const auto* result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    BEE_ASSERT_F(result != nullptr, "Failed to commit virtual memory: Win32 error code: %s", win32_get_last_error_string());
}


} // namespace bee
