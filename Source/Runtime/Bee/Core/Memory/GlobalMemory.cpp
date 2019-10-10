/*
 *  GlobalMemory.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Containers/Array.hpp>
#include "Bee/Core/Memory/MallocAllocator.hpp"
#include "Bee/Core/Memory/LinearAllocator.hpp"
#include "Bee/Core/Concurrency.hpp"

namespace bee {


Allocator* system_allocator() noexcept
{
    static MallocAllocator default_allocator;
    return &default_allocator;
}


struct TempAllocator final : public Allocator
{
    BEE_ALLOCATOR_DO_NOT_TRACK

    explicit TempAllocator(const size_t capacity) // NOLINT(cppcoreguidelines-pro-type-member-init,hicpp-member-init)
        : allocators(system_allocator()),
          capacity_(capacity)
    {}

    ~TempAllocator() override
    {
        scoped_spinlock_t lock(global_lock);

        for (auto& allocator : allocators)
        {
            allocator->destroy();
        }

        allocators.clear();
    }

    void reset()
    {
        scoped_spinlock_t lock(global_lock);
        for (auto allocator : allocators)
        {
            allocator->reset();
        }
    }

    bool is_valid(const void* ptr) const override
    {
        return thread_local_allocator.is_valid(ptr);
    }

    void* allocate(size_t size, size_t alignment) override
    {
        ensure_allocator();
        return thread_local_allocator.allocate(size, alignment);
    }

    void* reallocate(void* ptr, size_t old_size, size_t new_size, size_t alignment) override
    {
        ensure_allocator();
        return thread_local_allocator.reallocate(ptr, old_size, new_size, alignment);
    }

    void deallocate(void* ptr) override
    {
        ensure_allocator();
        thread_local_allocator.deallocate(ptr);
    }

private:
    size_t                              capacity_ { 0 };
    SpinLock                            global_lock;
    DynamicArray<LinearAllocator*>       allocators;

    char pad[64];

    static thread_local LinearAllocator  thread_local_allocator;

    void ensure_allocator()
    {
        if (thread_local_allocator.capacity() > 0)
        {
            return;
        }

        thread_local_allocator = LinearAllocator(capacity_);

        scoped_spinlock_t lock(global_lock);
        allocators.push_back(&thread_local_allocator);
    }
};

thread_local LinearAllocator TempAllocator::thread_local_allocator;

static TempAllocator temp_allocator_instance(BEE_CONFIG_DEFAULT_TEMP_ALLOCATOR_SIZE);

Allocator* temp_allocator() noexcept
{
    return &temp_allocator_instance;
}

void reset_temp_allocator() noexcept
{
    temp_allocator_instance.reset();
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