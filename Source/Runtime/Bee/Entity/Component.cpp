/*
 *  Component.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Entity/Component.hpp"
#include "Bee/Core/Memory/Memory.hpp"
#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

#include <string.h> // memcpy and memset


namespace bee {


ChunkAllocator::ChunkAllocator(const size_t chunk_size, const size_t chunk_alignment)
    : chunk_size_(chunk_size),
      chunk_alignment_(chunk_alignment)
{}

ChunkAllocator::~ChunkAllocator()
{
    while (first_ != nullptr)
    {
        auto current = first_->next;
        BEE_FREE(system_allocator(), first_);
        first_ = current;
    }
}

ChunkAllocator::AllocHeader* ChunkAllocator::get_alloc_header(void* ptr)
{
    BEE_ASSERT(ptr != nullptr);
    return reinterpret_cast<AllocHeader*>(static_cast<u8*>(ptr) - sizeof(AllocHeader));
}

const ChunkAllocator::AllocHeader* ChunkAllocator::get_alloc_header(const void* ptr)
{
    BEE_ASSERT(ptr != nullptr);
    return reinterpret_cast<const AllocHeader*>(static_cast<const u8*>(ptr) - sizeof(AllocHeader));
}

void* ChunkAllocator::allocate(const size_t size, const size_t alignment)
{
    BEE_ASSERT_F(chunk_size_ > 0, "Allocator has not been initialized");

    const auto last_size = last_ == nullptr ? 0 : last_->size;
    auto offset = round_up(last_size + sizeof(AllocHeader), alignment);

    // Use the currently available chunk
    if (last_ == nullptr || size <= chunk_size_ - offset - sizeof(ChunkHeader))
    {
        ChunkHeader* new_chunk = nullptr;

        // we've used up all the space - allocate a new chunk
        if (free_ != nullptr)
        {
            new_chunk = free_;
            free_ = free_->next;
        }
        else
        {
            new_chunk = static_cast<ChunkHeader*>(BEE_MALLOC_ALIGNED(system_allocator(), chunk_size_, chunk_alignment_));
        }

        new (new_chunk) ChunkHeader{};

        new_chunk->size = sizeof(ChunkHeader);
        new_chunk->data = reinterpret_cast<u8*>(new_chunk);

        offset = round_up(new_chunk->size + sizeof(AllocHeader), alignment);

        if (last_ == nullptr)
        {
            first_ = last_ = new_chunk;
        }
        else
        {
            last_->next = new_chunk;
            last_ = new_chunk;
        }
    }

    auto ptr = last_->data + offset;
    auto header = get_alloc_header(ptr);
    header->chunk = last_;
    header->size = size + sizeof(AllocHeader);
    last_->size = offset + size;

    if (BEE_FAIL_F(last_->size <= chunk_size_, "Cannot allocate more than %zu bytes", chunk_size_))
    {
        return nullptr;
    }

#if BEE_DEBUG == 1
    memset(ptr, uninitialized_alloc_pattern, size);
#endif // BEE_DEBUG == 1

    return ptr;
}

bool ChunkAllocator::is_valid(const void* ptr) const
{
    return get_alloc_header(ptr)->chunk->signature == header_signature;
}

void* ChunkAllocator::reallocate(void* ptr, const size_t old_size, const size_t new_size, const size_t alignment)
{
    if (old_size == new_size)
    {
        return ptr;
    }

    auto new_ptr = allocate(new_size, alignment);
    memcpy(new_ptr, ptr, old_size);
    deallocate(ptr);
    return new_ptr;
}

void ChunkAllocator::deallocate(void* ptr)
{
    auto header = get_alloc_header(ptr);

    if (BEE_FAIL(header->chunk->signature == header_signature))
    {
        return;
    }

    BEE_ASSERT(header->chunk->size >= sizeof(ChunkHeader) + header->size);

    header->chunk->size -= header->size;

    if (header->chunk->size <= sizeof(ChunkHeader) + sizeof(AllocHeader))
    {
        header->chunk->next = free_;
        free_ = header->chunk;
    }

#if BEE_DEBUG == 1
    memset(header, deallocated_memory_pattern, header->size);
#endif // BEE_DEBUG == 1
}

EntityComponentDependencyMap::EntityComponentDependencyMap()
    : allocator(kibibytes(4), alignof(DependencyInfo))
{}

void EntityComponentDependencyMap::add_type_if_not_registered(const Type* type)
{
    scoped_rw_write_lock_t lock(mutex);

    if (type_dependencies.find(type->hash) != nullptr)
    {
        return;
    }

    type_dependencies.insert(type->hash, BEE_NEW(allocator, DependencyInfo));
}

void EntityComponentDependencyMap::add_dependencies(const EntityComponentAccess access, JobGroup* group, const Span<const Type*>& read_types, const Span<const Type*>& write_types)
{
    auto deps = BEE_ALLOCA_ARRAY(DependencyInfo*, read_types.size() + write_types.size());
    int dep_index = 0;

    job_wait(&all_dependencies_.ro_deps);

    scoped_rw_read_lock_t lock(mutex);

    for (const Type* type : write_types)
    {
        auto dep_for_type = type_dependencies.find(type->hash);
        deps[dep_index++] = dep_for_type->value;
    }

    // Add all the read-only dependencies for read/write operations
    if (access == EntityComponentAccess::read_write)
    {
        for (const Type* type : read_types)
        {
            auto dep_for_type = type_dependencies.find(type->hash);
            deps[dep_index++] = dep_for_type->value;
        }
    }

    if (access == EntityComponentAccess::read_only)
    {
        all_dependencies_.ro_deps.add_dependency(group);
    }
    else
    {
        all_dependencies_.rw_deps.add_dependency(group);
    }
}


} // namespace bee