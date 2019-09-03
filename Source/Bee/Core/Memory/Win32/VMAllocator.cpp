//
//  VMAllocator.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 8/01/2019
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#include "Bee/Core/Memory/VMAllocator.hpp"
#include "Bee/Core/Math/Math.hpp"

#define BEE_MINWINDOWS_ARCH_ONLY
#include "Bee/Core/Win32/MinWindows.h"

#include <memoryapi.h>
#include <errhandlingapi.h>

namespace bee {


void* VMAllocator::allocate(const size_t size, const size_t /*alignment*/)
{
    auto ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    BEE_ASSERT_F(ptr != nullptr, "VMAllocator: unable to allocate virtual memory: Win32 error code: %lu", GetLastError());
    return ptr;
}

void VMAllocator::deallocate(void* ptr, const size_t size)
{
    const auto success = VirtualFree(ptr, size, MEM_RELEASE);
    BEE_ASSERT_F(success, "VMAllocator: unable to free memory: Win32 error code: %lu", GetLastError());
}

void* VMAllocator::reallocate(void* ptr, const size_t old_size, const size_t new_size, const size_t alignment)
{
    auto realloc_memory = allocate(new_size, alignment);
    if (BEE_CHECK_F(realloc_memory != nullptr, "VMAllocator: failed to reallocate memory"))
    {
        memcpy(realloc_memory, ptr, math::min(old_size, new_size));
        deallocate(ptr, old_size);
    }
    return realloc_memory;
}


} // namespace bee
