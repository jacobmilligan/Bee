/*
 *  ThreadSafePoolAllocator.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Memory/ChunkAllocator.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

namespace bee {


struct Header
{
    i32 thread { 0 };
};


ChunkAllocator::ChunkAllocator(const size_t chunk_size, const size_t chunk_alignment, const size_t reserve_chunk_count, const bool validate_leaks_on_destruct)
    : chunk_size_(chunk_size),
      chunk_alignment_(chunk_alignment),
      validate_on_destruct_(validate_leaks_on_destruct)
{

    for (int i = 0; i < reserve_chunk_count; ++i)
    {
        push_free(allocate_chunk());
    }
}

ChunkAllocator::ChunkAllocator(ChunkAllocator&& other) noexcept
{
    move_construct(other);
}

ChunkAllocator::~ChunkAllocator()
{
    BEE_ASSERT_F(!validate_on_destruct_ || last_ == nullptr, "Chunk allocator still has active allocations - this indicates a possible memory leak");

    // free all the nodes from the free stack
    while (free_ != nullptr)
    {
        auto* next = free_->next;
        BEE_FREE(system_allocator(), free_);
        free_ = next;
    }

    free_ = nullptr;
}

ChunkAllocator& ChunkAllocator::operator=(ChunkAllocator&& other) noexcept
{
    move_construct(other);
    return *this;
}

void ChunkAllocator::move_construct(ChunkAllocator& other) noexcept
{
    destruct(this);
    chunk_size_ = other.chunk_size_;
    chunk_alignment_ = other.chunk_alignment_;
    first_ = other.first_;
    last_ = other.last_;
    free_ = other.free_;
    other.chunk_size_ = 0;
    other.chunk_alignment_ = 0;
    other.first_ = nullptr;
    other.last_ = nullptr;
    other.free_ = nullptr;
}

bool ChunkAllocator::is_valid(const void* ptr) const
{
    return validate_allocation(ptr)->chunk->signature == header_signature_;
}

const ChunkAllocator::Allocation* ChunkAllocator::validate_allocation(const void *ptr) const
{
#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    BEE_ASSERT(ptr != nullptr);
    const auto* header = reinterpret_cast<const Allocation*>(static_cast<const u8*>(ptr) - sizeof(Allocation));
    BEE_ASSERT(header->chunk != nullptr && header->chunk->signature == header_signature_);
    return header;
#else
    return reinterpret_cast<const Allocation*>(static_cast<const u8*>(ptr) - sizeof(Allocation));
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1
}

ChunkAllocator::Allocation* ChunkAllocator::validate_allocation(void *ptr)
{
#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    BEE_ASSERT(ptr != nullptr);
    auto* header = reinterpret_cast<Allocation*>(static_cast<u8*>(ptr) - sizeof(Allocation));
    BEE_ASSERT(header->chunk != nullptr && header->chunk->signature == header_signature_);
    return header;
#else
    return reinterpret_cast<Allocation*>(static_cast<u8*>(ptr) - sizeof(Allocation));
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1
}

ChunkAllocator::Allocation* ChunkAllocator::get_allocation(void *ptr)
{
    BEE_ASSERT(ptr != nullptr);
    return reinterpret_cast<Allocation*>(static_cast<u8*>(ptr) - sizeof(Allocation));
}

ChunkAllocator::Chunk* ChunkAllocator::allocate_chunk() const
{
    auto* new_chunk = static_cast<Chunk*>(system_allocator()->allocate(chunk_size_));
    new (new_chunk) Chunk{};
    new_chunk->data = static_cast<void*>(new_chunk);

    return new_chunk;
}

void ChunkAllocator::push_free(Chunk* chunk)
{
    // unlink
    if (chunk->prev != nullptr)
    {
        chunk->prev->next = chunk->next;
    }

    if (chunk->next != nullptr)
    {
        chunk->next->prev = chunk->prev;
    }

    // remove from allocated list if first/last
    if (chunk == first_)
    {
        first_ = chunk->next;
    }
    if (chunk == last_)
    {
        last_ = last_->prev;
    }

    // add to the free list
    if (free_ == nullptr)
    {
        free_ = chunk;
    }
    else
    {
        chunk->next = free_;
        free_->prev = chunk;
        free_ = chunk;
    }
}

ChunkAllocator::Chunk* ChunkAllocator::pop_free()
{
    if (free_ == nullptr)
    {
        return nullptr;
    }

    auto* free = free_;
    free_ = free_->next;

    new (free) Chunk{};
    free->data = static_cast<void*>(free);

    return free;
}

void* ChunkAllocator::allocate(const size_t size, const size_t alignment)
{
    BEE_ASSERT(chunk_size_ > 0);
    BEE_ASSERT(size <= chunk_size_ - sizeof(Allocation) - sizeof(Chunk));

    const auto last_offset = last_ == nullptr ? 0 : last_->offset;
    auto offset = round_up(last_offset + sizeof(Allocation), alignment);

    if (last_ == nullptr || offset + size > chunk_size_ - sizeof(Chunk))
    {
        Chunk* new_chunk = pop_free();

        if (new_chunk == nullptr)
        {
            new_chunk = allocate_chunk();
        }

        if (last_ == nullptr)
        {
            first_ = last_ = new_chunk;
        }
        else
        {
            new_chunk->prev = last_;
            last_->next = new_chunk;
            last_ = new_chunk;
        }

        offset = round_up(new_chunk->offset + sizeof(Allocation), alignment);
    }

    auto* ptr = static_cast<u8*>(last_->data) + offset;
    auto* header = get_allocation(ptr);
    header->next = nullptr;
    header->chunk = last_;
    header->size = size;
    last_->size += size;
    last_->offset = offset + size;

    if (BEE_FAIL_F(last_->offset <= chunk_size_, "Cannot allocate more than %zu bytes", chunk_size_))
    {
        return nullptr;
    }

#if BEE_DEBUG == 1
    memset(ptr, uninitialized_alloc_pattern, size);
#endif // BEE_DEBUG == 1

    return ptr;
}

void* ChunkAllocator::reallocate(void* ptr, const size_t old_size, const size_t new_size, const size_t alignment)
{
    if (old_size == new_size)
    {
        return ptr;
    }

    auto* new_ptr = allocate(new_size, alignment);
    memcpy(new_ptr, ptr, old_size);
    deallocate(ptr);
    return new_ptr;
}

void ChunkAllocator::deallocate(void* ptr)
{
    auto* allocation = validate_allocation(ptr);
    auto* chunk = allocation->chunk;

    BEE_ASSERT(chunk->signature == header_signature_);
    BEE_ASSERT(chunk->size >= allocation->size);

    chunk->size -= allocation->size;

    if (chunk->size <= sizeof(Chunk) + sizeof(Allocation))
    {
        push_free(chunk);
    }

#if BEE_DEBUG == 1
    memset(ptr, deallocated_memory_pattern, allocation->size);
#endif // BEE_DEBUG == 1
}


} // namespace bee