/*
 *  LinearAllocator.hpp
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


class BEE_CORE_API LinearAllocator : public Allocator
{
public:
    using Allocator::allocate;

    BEE_ALLOCATOR_DO_NOT_TRACK

    LinearAllocator() = default;

    explicit LinearAllocator(const size_t capacity);

    LinearAllocator(LinearAllocator&& other) noexcept;

    ~LinearAllocator() override
    {
        destroy();
    }

    LinearAllocator& operator=(LinearAllocator&& other) noexcept;

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

    inline size_t offset() const
    {
        return offset_;
    }

    inline size_t capacity() const
    {
        return capacity_;
    }

    inline size_t allocated_size() const
    {
        return allocated_size_;
    }

    inline size_t min_allocation() const
    {
        return sizeof(size_t);
    }

    inline size_t max_allocation() const
    {
        return capacity_ - sizeof(size_t);
    }

    void destroy();

    void* allocate(const size_t size, const size_t alignment) override;

    void deallocate(void* ptr) override;

    void* reallocate(void* ptr, const size_t old_size, const size_t new_size, const size_t alignment) override;
private:
    size_t              offset_ { 0 };
    size_t              capacity_ { 0 };
    size_t              allocated_size_ { 0 }; // for i.e. job system where its only safe to reset if none of the memory is active
    u8*                 memory_{ nullptr };

    inline size_t get_header(void* ptr)
    {
        return *reinterpret_cast<size_t*>(static_cast<u8*>(ptr) - sizeof(size_t));
    }
};




} // namespace bee
