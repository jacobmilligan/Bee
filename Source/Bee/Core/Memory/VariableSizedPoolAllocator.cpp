/*
 *  VariableSizedPoolAllocator.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Random.hpp>
#include <Bee/Core/Hash.hpp>
#include "Bee/Core/Memory/VariableSizedPoolAllocator.hpp"
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Memory/Memory.hpp"

namespace bee {


VariableSizedPoolAllocator::VariableSizedPoolAllocator(const size_t min_allocation_size, const size_t max_allocation_size, const size_t max_items_per_chunk)
    : item_count_per_chunk_(max_items_per_chunk),
      chunk_count_(0),
      allocated_size_(0),
      capacity_(0)
{
    BEE_ASSERT_F(min_allocation_size > 0, "Min allocation size must be nonzero");
    BEE_ASSERT_F(max_allocation_size >= min_allocation_size, "Max allocation size must be at least equal to the min allocation size");

    for (size_t bucket_size = min_allocation_size; bucket_size <= max_allocation_size; bucket_size <<= 1u)
    {
        const auto item_byte_size = sizeof(Allocation) + bucket_size;
        capacity_ += sizeof(size_t) + sizeof(Chunk) + item_count_per_chunk_ * item_byte_size;
        ++chunk_count_;
    }

    for (size_t i = 1; i < BEE_ARCH_BITS; ++i)
    {
        if ((min_allocation_size >> i) == 0)
        {
            bucket_index_offset_ = i - 1;
            break;
        }
    }

    data_ = system_allocator()->allocate(capacity_);
    offsets_ = static_cast<size_t*>(data_);

    BEE_ASSERT(offsets_ != nullptr);

    reset();
}

VariableSizedPoolAllocator::~VariableSizedPoolAllocator()
{
    destroy();
}

VariableSizedPoolAllocator::VariableSizedPoolAllocator(VariableSizedPoolAllocator&& other) noexcept
{
    move_construct(other);
}

VariableSizedPoolAllocator& VariableSizedPoolAllocator::operator=(VariableSizedPoolAllocator&& other) noexcept
{
    move_construct(other);
    return *this;
}

void VariableSizedPoolAllocator::destroy()
{
    if (offsets_ == nullptr || data_ == nullptr)
    {
        return;
    }

    system_allocator()->deallocate(data_);
    offsets_ = nullptr;
    data_ = nullptr;
}

void VariableSizedPoolAllocator::move_construct(VariableSizedPoolAllocator& other) noexcept
{
    destroy();

    capacity_ = other.capacity_;
    item_count_per_chunk_ = other.item_count_per_chunk_;
    bucket_index_offset_ = other.bucket_index_offset_;
    chunk_count_ = other.chunk_count_;
    allocated_size_ = other.allocated_size_;
    offsets_ = other.offsets_;
    data_ = other.data_;

    other.capacity_ = 0;
    other.item_count_per_chunk_ = 0;
    other.bucket_index_offset_ = 0;
    other.chunk_count_ = 0;
    other.allocated_size_ = 0;
    other.offsets_ = nullptr;
    other.data_ = nullptr;
}

void VariableSizedPoolAllocator::Chunk::push_free(Allocation* item)
{
    item->next_allocation = free_list;
    free_list = item;
    allocated_count = allocated_count == 0 ? 0 : allocated_count - 1;
}

u32 VariableSizedPoolAllocator::Allocation::generate_signature() const
{
    const auto address = reinterpret_cast<size_t>(this);
    return get_hash(&address, sizeof(size_t), signature_seed_);
}

VariableSizedPoolAllocator::Allocation* VariableSizedPoolAllocator::Chunk::pop_free()
{
    if (free_list == nullptr)
    {
        return nullptr;
    }

    auto popped = free_list;
    free_list = popped->next_allocation;
    if (allocated_count < limits::max<size_t>())
    {
        ++allocated_count;
    }
    return popped;
}

const VariableSizedPoolAllocator::Allocation* VariableSizedPoolAllocator::get_allocation_from_ptr(const void* ptr) const
{
    auto allocation = reinterpret_cast<const Allocation*>(reinterpret_cast<const u8*>(ptr) - sizeof(Allocation));
    // validate the signature of the allocation
    if (allocation->signature != allocation->generate_signature())
    {
        return nullptr;
    }
    return allocation;
}

VariableSizedPoolAllocator::Allocation* VariableSizedPoolAllocator::get_allocation_from_ptr(void* ptr)
{
    return const_cast<VariableSizedPoolAllocator::Allocation*>(get_allocation_from_ptr(static_cast<const void*>(ptr)));
}

bool VariableSizedPoolAllocator::is_valid(const void* ptr) const
{
    return ptr != nullptr && get_allocation_from_ptr(ptr) != nullptr;
}

VariableSizedPoolAllocator::Chunk* VariableSizedPoolAllocator::get_chunk(const size_t size, const size_t alignment) const
{
    const auto allocated_size = math::to_next_pow2(round_up(size, alignment));
    BEE_ASSERT(allocated_size > 0);

    const auto pow2_idx = math::log2i(allocated_size);
    const auto bucket_idx = pow2_idx - bucket_index_offset_;

    BEE_ASSERT_F(pow2_idx >= bucket_index_offset_, "Allocation size was smaller than the given min_allocation_size for the pool: %zu", size);
    BEE_ASSERT_F(bucket_idx < chunk_count_, "Allocation size exceeds the pools given max_allocation_size: %zu (this can also be caused by an internal error)", size);

    if (pow2_idx < bucket_index_offset_ || bucket_idx >= chunk_count_)
    {
        return nullptr;
    }

    return reinterpret_cast<Chunk*>(static_cast<u8*>(data_) + offsets_[bucket_idx]);
}

void* VariableSizedPoolAllocator::allocate(const size_t size, const size_t alignment)
{
    // We store the free list pointers inside the allocations
    const auto chunk = get_chunk(size, alignment);
    if (BEE_FAIL_F(chunk != nullptr, "Invalid allocation size given to pool: %zu", size))
    {
        return nullptr;
    }

    auto next_available = chunk->pop_free();

    if (BEE_FAIL_F(next_available != nullptr, "Pool memory is exhausted for bucket with size: %zu", chunk->data_size))
    {
        return nullptr;
    }

    allocated_size_ += chunk->data_size;
    return next_available->data;
}

void* VariableSizedPoolAllocator::reallocate(void* ptr, const size_t old_size, const size_t new_size, const size_t alignment)
{
    const auto new_chunk = get_chunk(new_size, alignment);
    const auto old_chunk = get_chunk(old_size, alignment);

    BEE_ASSERT(old_chunk != nullptr);

    if (new_chunk == old_chunk)
    {
        return ptr;
    }

    if (BEE_FAIL_F(new_chunk != nullptr, "Allocation size exceeds the pools maximum possible allocation size: %zu", new_size))
    {
        return nullptr;
    }

    if (!is_valid(ptr))
    {
        return nullptr;
    }

    auto allocation = get_allocation_from_ptr(ptr);
    auto new_allocation = new_chunk->pop_free();

    if (new_allocation == nullptr)
    {
        return nullptr;
    }

    const auto memcpy_size = math::min(old_chunk->data_size, new_chunk->data_size);

    memcpy(new_allocation->data, allocation->data, memcpy_size);

    allocation->parent_chunk->push_free(allocation);
    allocated_size_ -= old_chunk->data_size;
    allocated_size_ += new_chunk->data_size;

    return new_allocation;
}

void VariableSizedPoolAllocator::deallocate(void* ptr)
{
    auto allocation = get_allocation_from_ptr(ptr);
    if (allocation == nullptr)
    {
        return;
    }

    const auto chunk_is_full = allocation->parent_chunk->allocated_count >= allocation->parent_chunk->capacity;
    if (BEE_FAIL_F(!chunk_is_full, "Pool memory is exhausted for bucket"))
    {
        return;
    }

    allocation->parent_chunk->push_free(allocation);
    if (allocated_size_ >= allocation->parent_chunk->data_size)
    {
        allocated_size_ -= allocation->parent_chunk->data_size;
    }
    else
    {
        allocated_size_ = 0;
    }
}

void VariableSizedPoolAllocator::reset()
{
    auto chunk_offset = sizeof(size_t) * chunk_count_;
    for (size_t bucket = 0; bucket < chunk_count_; ++bucket)
    {
        const auto item_data_size = 1u << (bucket + bucket_index_offset_);
        const auto item_allocation_size = sizeof(Allocation) + item_data_size;
        const auto capacity = sizeof(Chunk) + item_count_per_chunk_ * item_allocation_size;

        // setup a new Chunk
        auto chunk_bytes = static_cast<u8*>(data_) + chunk_offset;
        auto chunk = reinterpret_cast<Chunk*>(chunk_bytes);
        new (chunk) Chunk(capacity, item_data_size);

        offsets_[bucket] = chunk_offset;

        // Setup all the allocations, resetting the free list to contain all items in the chunk
        for (size_t item = 0; item < item_count_per_chunk_; ++item)
        {
            const auto is_last_in_list = item >= item_count_per_chunk_ - 1;
            auto allocation = reinterpret_cast<Allocation*>(chunk_bytes + sizeof(Chunk) + item * item_allocation_size);
            auto next_allocation = reinterpret_cast<Allocation*>(chunk_bytes + (item + 1) * item_allocation_size);
            new (allocation) Allocation(chunk, is_last_in_list ? nullptr : next_allocation);
            chunk->push_free(allocation);
        }

        chunk_offset += capacity;
    }

    allocated_size_ = 0;
}


} // namespace bee
