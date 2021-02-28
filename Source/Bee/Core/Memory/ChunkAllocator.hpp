/*
 *  ThreadSafePoolAllocator.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Concurrency.hpp"

namespace bee {


/**
 * Pools memory from pre-allocated chunks of a given size. When an allocation is requested, the last
 * active chunk will be checked to see if it has enough memory remaining and the allocation will then
 * increase the chunks allocated size. If this fails, a new chunk is allocated via malloc or popped from
 * a free list of previously deallocated chunks. When an allocation is deallocated it just decrements
 * its parent chunks size and when this reaches zero the chunk is returned to the free list.
 *
 * Allocations are not thread-safe
 */
class BEE_CORE_API ChunkAllocator final : public Allocator
{
public:
    ChunkAllocator() = default;

    ChunkAllocator(const size_t chunk_size, const size_t chunk_alignment, const size_t reserve_chunk_count, const bool validate_leaks_on_destruct = false);

    ChunkAllocator(ChunkAllocator&& other) noexcept;

    ~ChunkAllocator() override;

    ChunkAllocator& operator=(ChunkAllocator&& other) noexcept;

    bool is_valid(const void* ptr) const override;

    void* allocate(size_t size, size_t alignment) override;

    void* reallocate(void* ptr, size_t old_size, size_t new_size, size_t alignment) override;

    void deallocate(void* ptr) override;

private:
    static constexpr u32 header_signature_ = 0x73465829;

    struct Chunk
    {
        u32             signature { header_signature_ };
        Chunk*          next { nullptr };
        Chunk*          prev { nullptr };
        void*           data { nullptr };
        size_t          size { 0 };
        size_t          offset { sizeof(Chunk) };
    };

    struct Allocation
    {
        Allocation* next { nullptr };
        Chunk*      chunk { nullptr };
        size_t      size { 0 };
    };

    size_t  chunk_size_ { 0 };
    size_t  chunk_alignment_ { 0 };
    Chunk*  first_ { nullptr };
    Chunk*  last_ { nullptr };
    Chunk*  free_ { nullptr };
    bool    validate_on_destruct_ { false };

    const Allocation* validate_allocation(const void* ptr) const;

    Allocation* validate_allocation(void* ptr);

    Allocation* get_allocation(void* ptr);

    ChunkAllocator::Chunk* allocate_chunk() const;

    void push_free(Chunk* chunk);

    Chunk* pop_free();

    void move_construct(ChunkAllocator& other) noexcept;
};


} // namespace bee