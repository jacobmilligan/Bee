/*
 *  GlobalMemory.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Memory/MallocAllocator.hpp"
#include "Bee/Core/Memory/ThreadSafeLinearAllocator.hpp"
#include "Bee/Core/Concurrency.hpp"

namespace bee {


static MallocAllocator              g_system_allocator;
static ThreadSafeLinearAllocator    g_temp_allocator;


void global_allocators_init()
{
    new (&g_system_allocator) MallocAllocator{};
    new (&g_temp_allocator) ThreadSafeLinearAllocator(32, BEE_CONFIG_DEFAULT_TEMP_ALLOCATOR_SIZE, &g_system_allocator);
}

void global_allocators_shutdown()
{
    destruct(&g_temp_allocator);
    destruct(&g_system_allocator);
}

Allocator* system_allocator() noexcept
{
    return &g_system_allocator;
}

Allocator* temp_allocator() noexcept
{
    return &g_temp_allocator;
}

void temp_allocator_reset() noexcept
{
    g_temp_allocator.reset();
}

void temp_allocator_register_thread() noexcept
{
    g_temp_allocator.register_thread();
}

void temp_allocator_unregister_thread() noexcept
{
    g_temp_allocator.unregister_thread();
}


#if BEE_OS_WINDOWS == 0

constexpr isize Allocator::uninitialized_alloc_pattern;
constexpr isize Allocator::deallocated_memory_pattern;

#endif // BEE_OS_WINDOWS

} // namespace bee

void* operator new(const size_t size)
{
    return BEE_MALLOC(bee::system_allocator(), size);
}

void* operator new[](const size_t size)
{
    return BEE_MALLOC(bee::system_allocator(), size);
}

void operator delete(void* ptr) noexcept
{
    BEE_FREE(bee::system_allocator(), ptr);
}

void operator delete[](void* ptr) noexcept
{
    BEE_FREE(bee::system_allocator(), ptr);
}