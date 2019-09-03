//
//  PoolAllocator.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 27/06/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#include <Bee/Core/Math/Math.hpp>
#include "Bee/Core/Hash.hpp"
#include "Bee/Core/Memory/PoolAllocator.hpp"

namespace bee {


PoolAllocator::PoolAllocator(const size_t chunk_size, const size_t chunk_alignment, const size_t initial_chunk_count)
    : chunk_size_(chunk_size + sizeof(Header)),
      chunk_alignment_(chunk_alignment),
      allocated_chunk_count_(0)
{
    // Initialize new chunks by allocating and then deallocating them, putting them back onto the free list
    for (int chunk_idx = 0; chunk_idx < initial_chunk_count; ++chunk_idx)
    {
        allocate_chunk();
    }

    reset();
}

PoolAllocator::PoolAllocator(PoolAllocator&& other) noexcept
{
    move_construct(other);
}

PoolAllocator::~PoolAllocator()
{
    destroy();
}

PoolAllocator& PoolAllocator::operator=(PoolAllocator&& other) noexcept
{
    move_construct(other);
    return *this;
}

void PoolAllocator::destroy()
{
    auto chunk = first_chunk_;
    while (chunk != nullptr)
    {
        auto next = chunk->next_allocation;
#if BEE_DEBUG
        BEE_ASSERT(chunk->signature == get_header_signature(chunk));
#endif // BEE_DEBUG
        system_allocator()->deallocate(chunk);
        chunk = next;
    }

    free_list_ = first_chunk_ = last_chunk_ = nullptr;
}

void PoolAllocator::move_construct(bee::PoolAllocator& other) noexcept
{
    destroy();

    chunk_size_ = other.chunk_size_;
    chunk_alignment_ = other.chunk_alignment_;
    allocated_chunk_count_ = other.allocated_chunk_count_;
    first_chunk_ = other.first_chunk_;
    last_chunk_ = other.last_chunk_;
    free_list_ = other.free_list_;

    other.chunk_size_ = 0;
    other.chunk_alignment_ = 0;
    other.allocated_chunk_count_ = 0;
    other.first_chunk_ = nullptr;
    other.last_chunk_ = nullptr;
    other.free_list_ = nullptr;
}

PoolAllocator::Header* PoolAllocator::allocate_chunk()
{
    // Allocate a new chunk and add it to the free list
    auto header = static_cast<Header*>(system_allocator()->allocate(chunk_size_, chunk_alignment_));
    header->next_allocation = nullptr;
    header->next_free = nullptr;
    header->signature = get_header_signature(header);

    // First block allocated
    if (first_chunk_ == nullptr)
    {
        first_chunk_ = header;
    }
    else
    {
        // Otherwise just append to the block list
        last_chunk_->next_allocation = header;
    }

    last_chunk_ = header;
    ++allocated_chunk_count_;
    return header;
}

bool PoolAllocator::is_valid(const void* ptr) const
{
    if (ptr == nullptr)
    {
        return false;
    }

    auto header = get_header(ptr);
    return header->signature == get_header_signature(header);
}

void* PoolAllocator::allocate(size_t size, size_t alignment)
{
    BEE_ASSERT_F(round_up(size, alignment) <= chunk_size_, "Size requested exceeds the pools chunk size");

    if (free_list_ == nullptr)
    {
        // Allocate a new chunk and add it to the free list
        free_list_ = allocate_chunk();
        ++available_chunk_count_;
    }

    // Append to free list
    auto free_chunk = free_list_;
    free_list_ = free_list_->next_free;

    available_chunk_count_ = math::max(0, available_chunk_count_ - 1);
    return reinterpret_cast<u8*>(free_chunk) + sizeof(Header);
}

void* PoolAllocator::reallocate(void* ptr, size_t /* old_size */, size_t new_size, size_t alignment)
{
    BEE_ASSERT_F(round_up(new_size, alignment) <= chunk_size_, "Size requested exceeds the pools chunk size");
    BEE_ASSERT_F(ptr != nullptr, "Invalid pointer given to PoolAllocator::reallocate");
    return ptr;
}

void PoolAllocator::deallocate(void* ptr)
{
    if (BEE_FAIL_F(is_valid(ptr), "Trying to deallocate an invalid pointer"))
    {
        return;
    }

    auto header = get_header(ptr);

    if (free_list_ == nullptr)
    {
        free_list_ = header;
    }
    else
    {
        free_list_->next_free = header;
    }

     header->next_free = nullptr;
    ++available_chunk_count_;
}

void PoolAllocator::reset()
{
    auto current_allocation = first_chunk_;
    while (current_allocation != nullptr)
    {
        current_allocation->next_free = current_allocation->next_allocation;
        current_allocation = current_allocation->next_allocation;
        ++available_chunk_count_;
    }

    free_list_ = first_chunk_;
}

PoolAllocator::Header* PoolAllocator::get_header(void* ptr)
{
    return const_cast<Header*>(get_header(static_cast<const void*>(ptr)));
}

const PoolAllocator::Header* PoolAllocator::get_header(const void* ptr) const
{
    return reinterpret_cast<const Header*>(reinterpret_cast<const u8*>(ptr) - sizeof(Header));
}

u32 PoolAllocator::get_header_signature(const Header* header) const
{
    static constexpr u32 signature_seed = 0x23464829;
    const auto address = reinterpret_cast<size_t>(header);
    return get_hash(&address, sizeof(size_t), signature_seed);
}


}
