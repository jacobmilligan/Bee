/*
 *  LinearAllocator.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Memory/LinearAllocator.hpp"
#include "Bee/Core/Logger.hpp"

namespace bee {


size_t get_header(void* ptr)
{
    return *reinterpret_cast<size_t*>(static_cast<u8*>(ptr) - sizeof(size_t));
}

LinearAllocator::LinearAllocator(const size_t capacity)
    : offset_(0),
      capacity_(capacity),
      memory_(nullptr)
{
    memory_ = static_cast<u8*>(BEE_MALLOC(system_allocator(), capacity_));
}

LinearAllocator::LinearAllocator(const size_t capacity, Allocator* overflow_allocator)
    : LinearAllocator(capacity)
{
    overflow_ = overflow_allocator;
}

LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
    : offset_(other.offset_),
      capacity_(other.capacity_),
      memory_(other.memory_),
      overflow_(other.overflow_)
{
    other.offset_ = 0;
    other.capacity_ = 0;
    other.memory_ = nullptr;
    other.overflow_ = nullptr;
}

LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept
{
    destroy();
    offset_ = other.offset_;
    capacity_ = other.capacity_;
    memory_ = other.memory_;
    overflow_ = other.overflow_;
    other.offset_ = 0;
    other.capacity_ = 0;
    other.memory_ = nullptr;
    other.overflow_ = nullptr;
    return *this;
}

void LinearAllocator::destroy()
{
    if (memory_ == nullptr)
    {
        return;
    }

    if (allocated_overflow_ != 0)
    {
        log_error("LinearAllocator: not all allocations were freed before destroying the allocator (allocated_size_ = %zu)", allocated_overflow_);
    }

    BEE_FREE(system_allocator(), memory_);
    memory_ = nullptr;
}

bool LinearAllocator::is_overflow_memory(void* ptr)
{
    const auto as_bytes = static_cast<u8*>(ptr);
    return as_bytes < memory_ || as_bytes > (memory_ + capacity_);
}

void* LinearAllocator::allocate(const size_t size, const size_t alignment)
{
    BEE_ASSERT(memory_ != nullptr);

    constexpr auto header_size = sizeof(size_t);
    const auto new_offset = round_up(offset_ + header_size, alignment);
    const auto reached_capacity = new_offset + size > capacity_;
    u8* new_memory = nullptr;

    if (reached_capacity)
    {
        if (BEE_FAIL_F(overflow_ != nullptr, "Linear allocator has reached capacity (%zu >= %zu)", new_offset + size, capacity_))
        {
            return nullptr;
        }

        new_memory = static_cast<u8*>(BEE_MALLOC_ALIGNED(overflow_, size + header_size, alignment));
        allocated_overflow_ += size;
    }
    else
    {
        new_memory = memory_ + new_offset;
        offset_ = new_offset + size;
    }

    auto header = reinterpret_cast<size_t*>(new_memory - header_size);
    *header = size;

    allocated_size_ += size;

    return new_memory;
}

void LinearAllocator::deallocate(void* ptr)
{
    BEE_ASSERT(ptr != nullptr);

    const auto header = get_header(ptr);

    BEE_ASSERT(allocated_size_ >= header);

    allocated_size_ -= get_header(ptr);

    if (is_overflow_memory(ptr))
    {
        BEE_FREE(overflow_, ptr);
        allocated_overflow_ -= get_header(ptr);
    }
}

void* LinearAllocator::reallocate(void* ptr, const size_t old_size, const size_t new_size, const size_t alignment)
{
    BEE_ASSERT(is_valid(ptr));
    BEE_ASSERT(get_header(ptr) == old_size);

    auto realloc_memory = allocate(new_size, alignment);
    allocated_size_ -= old_size;

    if (BEE_CHECK_F(realloc_memory != nullptr, "LinearAllocator: failed to reallocate memory"))
    {
        memcpy(realloc_memory, ptr, math::min(old_size, new_size));
    }

    if (is_overflow_memory(ptr))
    {
        allocated_overflow_ -= old_size;
        BEE_FREE(overflow_, ptr);
    }

    return realloc_memory;
}


} // namespace bee