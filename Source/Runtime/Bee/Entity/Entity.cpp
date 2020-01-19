/*
 *  Entity.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Entity/Entity.hpp"

#include <algorithm>


namespace bee {


Archetype::Archetype(const u32 sorted_type_hash, const Span<const Type* const>& sorted_types, const size_t entity_size, Allocator* allocator)
    : hash(sorted_type_hash),
      entity_size(0),
      types(sorted_types, allocator),
      offsets(sorted_types.size(), 0, allocator)
{}

ChunkAllocator::ChunkAllocator(const size_t chunk_size, const size_t chunk_alignment)
    : chunk_size_(chunk_size),
      chunk_alignment_(chunk_alignment)
{

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
    auto offset = round_up(last_->size + sizeof(AllocHeader), alignment);
    const auto remaining_chunk_space = chunk_size_ - offset;

    // Use the currently available chunk
    if (last_ == nullptr || size > remaining_chunk_space)
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
            new_chunk = static_cast<ChunkHeader*>(system_allocator()->allocate(chunk_size_, chunk_alignment_));
        }

        new (new_chunk) ChunkHeader{};

        new_chunk->size = sizeof(ChunkHeader);
        new_chunk->data = reinterpret_cast<u8*>(new_chunk) + sizeof(ChunkHeader);
        offset = round_up(sizeof(ChunkHeader) + sizeof(AllocHeader), alignment);

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

    auto ptr = reinterpret_cast<u8*>(last_) + offset;
    auto header = get_alloc_header(ptr);

    header->chunk = last_;
    header->size = size + sizeof(AllocHeader);
    last_->size = offset + size;

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
    auto header = reinterpret_cast<AllocHeader*>(reinterpret_cast<u8*>(ptr) - sizeof(AllocHeader));

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
}



World::World(const WorldDescriptor& desc)
    : entities_(desc.entity_pool_chunk_size * sizeof(Entity)),
      archetype_allocator_(kibibytes(64), alignof(Archetype)),
      component_allocator_(kibibytes(64), alignof(ComponentChunk))
{

}

Entity World::create_entity()
{
    Entity entity{};
    create_entities(&entity, 1);
    return entity;
}

void World::create_entities(Entity* dst, const i32 count)
{
    BEE_ASSERT(dst != nullptr);
    BEE_ASSERT(count > 0);

    for (int e = 0; e < count; ++e)
    {
        dst[e] = entities_.allocate();
        BEE_ASSERT(dst[e].is_valid());
    }
}

void World::destroy_entity(const Entity& entity)
{
    destroy_entities(&entity, 1);
}

void World::destroy_entities(const Entity* to_destroy, const i32 count)
{
    BEE_ASSERT(to_destroy != nullptr);
    BEE_ASSERT(count > 0);

    for (int e = 0; e < count; ++e)
    {
        entities_.deallocate(to_destroy[e]);
    }
}

Archetype* World::get_or_create_archetype(const Span<const Type* const>& sorted_types)
{
    const auto archetype_hash = get_hash(sorted_types.data(), sorted_types.byte_size(), 0xF00D);
    auto mapped = archetype_lookup_.find(archetype_hash);
    if (mapped != nullptr)
    {
        return mapped->value;
    }

    // Entity is always the first component so we can look it up later
    auto entity_size = sizeof(Entity);
    for (const Type* const type : sorted_types)
    {
        entity_size += type->size;
    }

    auto archetype = BEE_NEW(archetype_allocator_, Archetype)(archetype_hash, sorted_types, entity_size, &archetype_allocator_);

    size_t current_offset = 0;

    for (int t = 0; t < archetype->types.size(); ++t)
    {
        archetype->offsets[t] = current_offset;
        current_offset += archetype->types[t]->size;
    }

    archetype->chunk_size = component_allocator_.max_allocation_size(); // TODO(Jacob): allow variable-sized archetypes
    archetype->first_chunk = archetype->last_chunk = create_chunk(archetype);
    return archetype;
}

void World::destroy_archetype(Archetype* archetype)
{
    while (archetype->first_chunk != nullptr)
    {
        auto next_chunk = archetype->first_chunk->next;
        destroy_chunk(archetype->first_chunk);
        archetype->first_chunk = next_chunk;
    }

    BEE_DELETE(archetype_allocator_, archetype);
}

ComponentChunk* World::create_chunk(Archetype* archetype)
{
    auto ptr = static_cast<u8*>(BEE_MALLOC_ALIGNED(component_allocator_, archetype->chunk_size, alignof(ComponentChunk)));
    auto chunk = reinterpret_cast<ComponentChunk*>(ptr);
    chunk->next = nullptr;
    chunk->allocated_size = archetype->chunk_size;
    chunk->bytes_per_entity = archetype->entity_size;
    chunk->capacity = chunk->bytes_per_entity / (chunk->allocated_size - sizeof(ComponentChunk));
    chunk->count = 0;
    chunk->archetype = archetype;
    chunk->data = ptr + sizeof(ComponentChunk);
    return chunk;
}

void World::destroy_chunk(ComponentChunk* chunk)
{
    if (chunk->previous != nullptr)
    {
        chunk->previous->next = chunk->next;
    }

    if (chunk->next != nullptr)
    {
        chunk->next->previous = chunk->previous;
    }

    if (chunk->archetype->first_chunk == chunk)
    {
        chunk->archetype->first_chunk = chunk->next;
    }

    if (chunk->archetype->last_chunk == chunk)
    {
        chunk->archetype->last_chunk = chunk->previous;
    }

    BEE_FREE(component_allocator_, chunk);
}

void World::destroy_entity(EntityInfo* info)
{
    BEE_ASSERT(info->index_in_chunk < info->chunk->count);

    auto archetype = info->chunk->archetype;
    auto entity = reinterpret_cast<Entity*>(info->chunk->data + sizeof(Entity) * info->index_in_chunk);
    auto last_chunk = archetype->last_chunk;
    auto old_chunk = info->chunk;

    // If this is not last entity in the chunk we have to swap it with the last one in the archetype
    if (info->index_in_chunk != info->chunk->count - 1)
    {
        // Copy the last entities components and then decrement the count of that chunk
        copy_components_in_chunks(info->chunk, info->index_in_chunk, last_chunk, last_chunk->count - 1);
        old_chunk = last_chunk;
    }

    --old_chunk->count;

    if (old_chunk->count <= 0)
    {
        destroy_chunk(old_chunk);

        if (archetype->first_chunk == nullptr)
        {
            destroy_archetype(archetype);
        }
    }
    else
    {
        new (entity) Entity{};
        info->index_in_chunk = -1;
        info->chunk = nullptr;
    }
}

void World::move_entity(EntityInfo* info, Archetype* dst)
{
    BEE_ASSERT(dst->first_chunk != nullptr);
    BEE_ASSERT(dst->last_chunk != nullptr);

    // Allocate a new chunk if the archetype is full
    if (dst->last_chunk->count >= dst->last_chunk->capacity)
    {
        auto new_chunk = create_chunk(dst);
        dst->last_chunk->next = new_chunk;
        dst->last_chunk = new_chunk;
    }

    // Copy the components from the old to the new chunk - last is always the only empty chunk
    copy_components_in_chunks(dst->last_chunk, dst->last_chunk->count, info->chunk, info->index_in_chunk);
    destroy_entity(info);

    ++dst->last_chunk->count;
    info->index_in_chunk = dst->last_chunk->count;
    info->chunk = dst->last_chunk;
}

bool World::has_component(const EntityInfo* info, const Type* type)
{
    for (const Type* const stored_type : info->chunk->archetype->types)
    {
        if (stored_type == type)
        {
            return true;
        }
    }
    return false;
}

static thread_local const Type* local_type_array[128];

Span<const Type* const> get_sorted_type_array_additive(const Span<const Type* const>& old_types, const Type* added_type)
{
    if (BEE_FAIL(old_types.size() + 1 < static_array_length(local_type_array)))
    {
        return Span<const Type* const>{};
    }

    memcpy(local_type_array, old_types.data(), old_types.byte_size());
    local_type_array[old_types.size()] = added_type;
    std::sort(local_type_array, local_type_array + old_types.size() + 1);
    return Span<const Type* const>(local_type_array);
}

Span<const Type* const> get_sorted_type_array_subtractive(const Span<const Type* const>& old_types, const Type* removed_type)
{
    if (old_types.size() - 1 <= 0)
    {
        return Span<const Type* const>{};
    }

    memcpy(local_type_array, old_types.data(), old_types.byte_size());

    int type_index = -1;

    for (int t = 0; t < old_types.size(); ++t)
    {
        if (local_type_array[t]->hash == removed_type->hash)
        {
            type_index = t;
            break;
        }
    }

    if (type_index >= 0 && type_index < old_types.size() - 1)
    {
        std::swap(local_type_array[type_index], local_type_array[old_types.size()]);
    }

    return Span<const Type* const>(local_type_array, old_types.size() - 1);
}

void copy_components_in_chunks(ComponentChunk* dst, const i32 dst_index, const ComponentChunk* src, const i32 src_index)
{
    BEE_ASSERT_F(src_index >= 0 && dst_index >= 0, "Invalid index given to copy_components");

    const auto src_archetype = src->archetype;
    auto dst_archetype = dst->archetype;
    auto src_type_iter = src_archetype->types.begin();
    auto dst_type_iter = dst_archetype->types.begin();
    auto src_offset_iter = src_archetype->offsets.begin();
    auto dst_offset_iter = dst_archetype->offsets.begin();
    void* dst_component = nullptr;
    void* src_component = nullptr;

    /*
     * We can take advantage of the fact that the type arrays are always sorted by hash and
     * keep stepping through, only incrementing each iterator when one has a smaller hash than the other
     */
    while (src_type_iter != src_archetype->types.end() && dst_type_iter != dst_archetype->types.end())
    {
        // copy over types that are the same
        if ((*src_type_iter)->hash == (*dst_type_iter)->hash)
        {
            dst_component = dst->data + *dst_offset_iter + (*dst_type_iter)->size * dst_index;
            src_component = src->data + *src_offset_iter + (*src_type_iter)->size * src_index;
            memcpy(dst_component, src_component, (*dst_type_iter)->size);

            ++dst_type_iter;
            ++dst_offset_iter;
            ++src_type_iter;
            ++src_offset_iter;
            continue;
        }

        // Skip over any components from the source chunk that the destination chunk doesn't have
        if ((*src_type_iter)->hash < (*dst_type_iter)->hash)
        {
            ++src_type_iter;
            ++src_offset_iter;
            continue;
        }

        // zero out any components that the destination chunk has that the source chunk doesn't - similar to constructing the data
        if ((*src_type_iter)->hash > (*dst_type_iter)->hash)
        {
            dst_component = dst->data + *dst_offset_iter + (*dst_type_iter)->size * dst_index;
            memset(dst_component, 0, (*dst_type_iter)->size);
            ++dst_type_iter;
            ++dst_offset_iter;
        }
    }
}


} // namespace bee