/*
 *  VariableSizedPoolAllocator.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/NumericTypes.hpp"

namespace bee {


class BEE_CORE_API VariableSizedPoolAllocator : public Allocator
{
public:
    using Allocator::allocate;

    VariableSizedPoolAllocator() = default;

    VariableSizedPoolAllocator(size_t min_allocation_size, size_t max_allocation_size, size_t max_items_per_chunk);

    VariableSizedPoolAllocator(VariableSizedPoolAllocator&& other) noexcept;

    ~VariableSizedPoolAllocator() override;

    VariableSizedPoolAllocator& operator=(VariableSizedPoolAllocator&& other) noexcept;

    bool is_valid(const void* ptr) const override;

    void reset();

    inline size_t allocated_size() const
    {
        return allocated_size_;
    }

    inline size_t capacity() const
    {
        return capacity_;
    }

    inline size_t chunk_count() const
    {
        return chunk_count_;
    }

    inline size_t item_count_per_chunk() const
    {
        return item_count_per_chunk_;
    }

    void* allocate(size_t size, size_t alignment) override;

    void* reallocate(void* ptr, size_t old_size, size_t new_size, size_t alignment) override;

    void deallocate(void* ptr) override;

private:
    struct Chunk;

    struct Allocation
    {
        Allocation(Chunk* new_parent, Allocation* initial_next)
            : parent_chunk(new_parent),
              next_allocation(initial_next),
              signature(generate_signature()),
              data(reinterpret_cast<u8*>(this) + sizeof(Allocation))
        {}

        Chunk*      parent_chunk { nullptr };
        Allocation* next_allocation { nullptr };
        u32         signature { 0 };
        void*       data { nullptr };

        u32 generate_signature() const;
    };

    struct Chunk
    {
        Chunk(const size_t new_capacity, const size_t new_data_size)
            : capacity(new_capacity),
              data_size(new_data_size),
              free_list(nullptr),
              data(reinterpret_cast<Allocation*>(reinterpret_cast<u8*>(this) + sizeof(Chunk)))
        {}

        size_t          capacity { 0 };
        size_t          data_size { 0 };
        size_t          allocated_count { 0 };
        Allocation*     data { nullptr };
        Allocation*     free_list { nullptr };

        Allocation* pop_free();

        void push_free(Allocation* item);
    };

    static constexpr u32 signature_seed_ = 0x23464829;

    size_t  capacity_ { 0 };
    size_t  item_count_per_chunk_ { 0 };
    size_t  bucket_index_offset_ { 0 };
    size_t  chunk_count_ { 0 };
    size_t  allocated_size_ { 0 };
    size_t* offsets_ { nullptr };
    void*   data_ { nullptr };

    void destroy();

    void move_construct(VariableSizedPoolAllocator& other) noexcept;

    Allocation* get_allocation_from_ptr(void* ptr);

    const Allocation* get_allocation_from_ptr(const void* ptr) const;

    Chunk* get_chunk(const size_t size, const size_t alignment) const;
};


} // namespace bee
