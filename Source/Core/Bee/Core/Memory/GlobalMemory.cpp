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


// Global system allocator
static MallocAllocator          g_system_allocator;


// Temp allocators
struct PerThreadTempAllocator
{
    PerThreadTempAllocator*     next { nullptr };
    u64                         thread_id { limits::max<u64>() };
    ThreadSafeLinearAllocator   allocators[BEE_CONFIG_TEMP_ALLOCATOR_FRAME_COUNT];

    inline bool is_valid()
    {
        return thread_id < limits::max<u64>();
    }
};

struct TempAllocator
{
    PerThreadTempAllocator  per_thread[BEE_CONFIG_TEMP_ALLOCATOR_MAX_THREADS];
    std::atomic_int32_t     current_frame { 0 };
    PerThreadTempAllocator* next_allocator { nullptr };
    RecursiveSpinLock       mutex;

    TempAllocator() noexcept
    {
        scoped_recursive_spinlock_t lock(mutex);

        for (int i = 0; i < static_array_length(per_thread); ++i)
        {
            if (i >= static_array_length(per_thread) - 1)
            {
                continue;
            }

            per_thread[i].next = &per_thread[i + 1];
        }

        next_allocator = &per_thread[0];
    }

    ~TempAllocator()
    {
        scoped_recursive_spinlock_t lock(mutex);

        for (auto& t : per_thread)
        {
            if (t.is_valid())
            {
                destruct(&t);
            }
        }
    }

    PerThreadTempAllocator* obtain_per_thread()
    {
        if (BEE_FAIL_F(next_allocator != nullptr, "TempAllocator: More than BEE_CONFIG_TEMP_ALLOCATOR_MAX_THREADS were registered"))
        {
            return nullptr;
        }

        scoped_recursive_spinlock_t lock(mutex);

        auto this_thread = next_allocator;

        for (auto& allocator : this_thread->allocators)
        {
            new (&allocator) ThreadSafeLinearAllocator(BEE_CONFIG_DEFAULT_TEMP_ALLOCATOR_SIZE, &g_system_allocator);
        }

        this_thread->thread_id = current_thread::id();
        next_allocator = this_thread->next;
        return this_thread;
    }

    void release_per_thread(PerThreadTempAllocator* allocator)
    {
        if (BEE_FAIL_F(allocator != nullptr, "TempAllocator: thread was not registered for temp allocations"))
        {
            return;
        }

        scoped_recursive_spinlock_t lock(mutex);

        destruct(allocator);

        new (allocator) PerThreadTempAllocator{};
        allocator->next = next_allocator;
        next_allocator = allocator;
    }

    void reset()
    {
        // increment frame
        auto current = current_frame.load(std::memory_order_relaxed);
        auto next = (current + 1) % BEE_CONFIG_TEMP_ALLOCATOR_FRAME_COUNT;

        if (current_frame.compare_exchange_strong(current, next))
        {
            // reset current allocators
            for (auto& t : per_thread)
            {
                if (t.is_valid())
                {
                    t.allocators[next].reset();
                }
            }
        }
    }

    inline ThreadSafeLinearAllocator* get_current_frame(PerThreadTempAllocator* allocator)
    {
        return &allocator->allocators[current_frame.load(std::memory_order_relaxed)];
    }
};

static TempAllocator                        g_temp_allocator;
static thread_local PerThreadTempAllocator* g_local_temp_allocator { nullptr };


void global_allocators_init()
{
    new (&g_system_allocator) MallocAllocator{};
    new (&g_temp_allocator) TempAllocator{};
}

void global_allocators_shutdown()
{
    destruct(&temp_allocator);
    destruct(&g_system_allocator);
}

Allocator* system_allocator() noexcept
{
    return &g_system_allocator;
}

Allocator* temp_allocator() noexcept
{
    BEE_ASSERT_F(g_local_temp_allocator != nullptr, "TempAllocator: thread is not registered");
    BEE_ASSERT_F(g_local_temp_allocator->is_valid(), "TempAllocator: thread is not registered");
    return g_temp_allocator.get_current_frame(g_local_temp_allocator);
}

void temp_allocator_reset() noexcept
{
    g_temp_allocator.reset();
}

void temp_allocator_register_thread() noexcept
{
    if (BEE_FAIL_F(g_local_temp_allocator == nullptr || !g_local_temp_allocator->is_valid(), "Thread is already registered for temporary allocations"))
    {
        return;
    }

    g_local_temp_allocator = g_temp_allocator.obtain_per_thread();
}

void temp_allocator_unregister_thread() noexcept
{
    if (BEE_FAIL_F(g_local_temp_allocator != nullptr && g_local_temp_allocator->is_valid(), "Thread is not registered for temporary allocations"))
    {
        return;
    }

    g_temp_allocator.release_per_thread(g_local_temp_allocator);
    g_local_temp_allocator = nullptr;
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