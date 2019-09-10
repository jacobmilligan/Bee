/*
 *  LinearAllocator.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Memory/LinearAllocator.hpp"

namespace bee {


LinearAllocator::LinearAllocator(const size_t capacity)
    : offset_(0),
      capacity_(capacity),
      memory_(nullptr)
{
    memory_ = static_cast<u8*>(BEE_MALLOC(system_allocator(), capacity_));
}

LinearAllocator::LinearAllocator(LinearAllocator&& other) noexcept
    : offset_(other.offset_),
      capacity_(other.capacity_),
      memory_(other.memory_)
{
    other.offset_ = 0;
    other.capacity_ = 0;
    other.memory_ = nullptr;
}

LinearAllocator& LinearAllocator::operator=(LinearAllocator&& other) noexcept
{
    destroy();
    offset_ = other.offset_;
    capacity_ = other.capacity_;
    memory_ = other.memory_;
    other.offset_ = 0;
    other.capacity_ = 0;
    other.memory_ = nullptr;
    return *this;
}

void LinearAllocator::destroy()
{
    if (memory_ == nullptr)
    {
        return;
    }
    BEE_FREE(system_allocator(), memory_);
    memory_ = nullptr;
}

void* LinearAllocator::allocate(const size_t size, const size_t alignment)
{
    BEE_ASSERT(memory_ != nullptr);

    constexpr auto header_size = sizeof(size_t);
    const auto new_offset = round_up(offset_ + header_size, alignment);

    if (BEE_FAIL_F(new_offset + size <= capacity_, "LinearAllocator: reached capacity"))
    {
        return nullptr;
    }

    auto new_memory = memory_ + new_offset;
    auto header = reinterpret_cast<size_t*>(new_memory - header_size);
    *header = size;

    allocated_size_ += size;
    offset_ = new_offset + size;

    return new_memory;
}

void LinearAllocator::deallocate(void* ptr)
{
    BEE_ASSERT(ptr != nullptr);

    const auto header = get_header(ptr);

    BEE_ASSERT(allocated_size_ >= header);

    allocated_size_ -= get_header(ptr);
}

void* LinearAllocator::reallocate(void* ptr, const size_t old_size, const size_t new_size, const size_t alignment)
{
    BEE_ASSERT(is_valid(ptr));
    BEE_ASSERT(get_header(ptr) == old_size);

    auto realloc_memory = allocate(new_size, alignment);
    if (BEE_CHECK_F(realloc_memory != nullptr, "LinearAllocator: failed to reallocate memory"))
    {
        memcpy(realloc_memory, ptr, math::min(old_size, new_size));
    }
    return realloc_memory;
}


} // namespace bee