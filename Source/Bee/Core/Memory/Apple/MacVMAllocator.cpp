/*
 *  MacVMAllocator.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Memory/VMAllocator.hpp"
#include "Bee/Core/Memory/Memory.hpp"

#include <sys/mman.h>

namespace bee {


const size_t VMAllocator::page_size = get_page_size();


void* VMAllocator::allocate(const size_t size, const size_t /* alignment */)
{
    constexpr auto protection = PROT_READ | PROT_WRITE;
    constexpr auto map_flags = MAP_PRIVATE | MAP_ANON;
    constexpr auto null_fd = -1;

    return mmap(nullptr, size * page_size, protection, map_flags, null_fd, 0);
}

void VMAllocator::deallocate(void* ptr, const size_t size)
{
    BEE_ASSERT_F(is_valid(ptr), "VMAllocator: Attempted to deallocate an invalid page block");

    const auto munmap_success = munmap(ptr, size);
    BEE_ASSERT_F(munmap_success == 0, "VMAllocator: failed to deallocate page block: %s",
               strerror(munmap_success));
}



} // namespace bee