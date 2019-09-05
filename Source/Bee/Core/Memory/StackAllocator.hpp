/*
 *  StackAllocator.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/MallocAllocator.hpp"
#include "Bee/Core/Memory/Memory.hpp"
#include "Bee/Core/Math/Math.hpp"

#include <string.h>

namespace bee {


class StackAllocator : public Allocator
{
public:
    using Allocator::allocate;

    BEE_ALLOCATOR_DO_NOT_TRACK

    StackAllocator() = default;

    explicit StackAllocator(const size_t capacity)
        : offset_(0),
          capacity_(capacity),
          memory_(nullptr)
    {
        memory_ = static_cast<u8*>(BEE_MALLOC(system_allocator(), capacity_));
    }

    StackAllocator(StackAllocator&& other) noexcept
        : offset_(other.offset_),
          capacity_(other.capacity_),
          memory_(other.memory_)
    {
        other.offset_ = 0;
        other.capacity_ = 0;
        other.memory_ = nullptr;
    }

    ~StackAllocator() override
    {
        destroy();
    }

    StackAllocator& operator=(StackAllocator&& other) noexcept
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

    inline void reset()
    {
        offset_ = 0;
    }

    inline bool is_valid(const void* ptr) const override
    {
        return (memory_ == nullptr && ptr == nullptr ) || (ptr >= memory_ && ptr < memory_ + capacity_);
    }

    inline const u8* data() const
    {
        return memory_;
    }

    inline size_t cursor() const
    {
        return offset_;
    }

    inline size_t capacity() const
    {
        return capacity_;
    }

    inline void destroy()
    {
        if (memory_ == nullptr)
        {
            return;
        }
        BEE_FREE(system_allocator(), memory_);
        memory_ = nullptr;
    }

    void* allocate(const size_t size, const size_t alignment) override
    {
        BEE_ASSERT(memory_ != nullptr);

        constexpr auto header_size = sizeof(size_t);
        const auto new_offset = round_up(offset_ + header_size, alignment);

        if (BEE_FAIL_F(new_offset + size <= capacity_, "StackAllocator: reached capacity"))
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

    void deallocate(void* ptr) override
    {
        BEE_ASSERT(ptr != nullptr);

        const auto header = get_header(ptr);

        BEE_ASSERT(allocated_size_ >= header);

        allocated_size_ -= get_header(ptr);
    }

    void* reallocate(void* ptr, const size_t old_size, const size_t new_size, const size_t alignment) override
    {
        BEE_ASSERT(is_valid(ptr));
        BEE_ASSERT(get_header(ptr) == old_size);

        auto realloc_memory = allocate(new_size, alignment);
        if (BEE_CHECK_F(realloc_memory != nullptr, "StackAllocator: failed to reallocate memory"))
        {
            memcpy(realloc_memory, ptr, math::min(old_size, new_size));
        }
        return realloc_memory;
    }

    inline size_t allocated_size() const
    {
        return allocated_size_;
    }
private:
    size_t              offset_ { 0 };
    size_t              capacity_ { 0 };
    size_t              allocated_size_ { 0 }; // for i.e. job system where its only safe to reset if none of the memory is active
    u8*                 memory_{ nullptr };

    size_t get_header(void* ptr)
    {
        return *reinterpret_cast<size_t*>(static_cast<u8*>(ptr) - sizeof(size_t));
    }
};


} // namespace bee
