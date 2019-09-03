/*
 *  PoolAllocator.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/MallocAllocator.hpp"

namespace bee {


class BEE_CORE_API PoolAllocator : public Allocator
{
public:
    using Allocator::allocate;

    PoolAllocator() = default;

    PoolAllocator(size_t chunk_size, size_t chunk_alignment, size_t initial_chunk_count);

    PoolAllocator(PoolAllocator&& other) noexcept;

    ~PoolAllocator();

    PoolAllocator& operator=(PoolAllocator&& other) noexcept;

    bool is_valid(const void* ptr) const override;

    void reset();

    inline i32 allocated_chunk_count() const
    {
        return allocated_chunk_count_;
    }

    inline i32 available_chunk_count() const
    {
        return available_chunk_count_;
    }

    void* allocate(size_t size, size_t alignment) override;

    void* reallocate(void* ptr, size_t old_size, size_t new_size, size_t alignment) override;

    void deallocate(void* ptr) override;

private:
    struct Header
    {
        u32     signature { 0 };
        Header* next_allocation { nullptr };
        Header* next_free { nullptr };
    };

    size_t          chunk_size_ { 0 };
    size_t          chunk_alignment_ { 0 };
    i32             allocated_chunk_count_ { 0 };
    i32             available_chunk_count_ { 0 };
    Header*         first_chunk_ { nullptr };
    Header*         last_chunk_ { nullptr };
    Header*         free_list_ { nullptr };

    void destroy();

    void move_construct(PoolAllocator& other) noexcept;

    Header* allocate_chunk();

    Header* get_header(void* ptr);

    const Header* get_header(const void* ptr) const;

    u32 get_header_signature(const Header* header) const;
};


} // namespace bee




