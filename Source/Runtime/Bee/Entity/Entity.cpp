/*
 *  Entity.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Entity/Entity.hpp"

#include <algorithm>


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
        system_allocator()->deallocate(first_);
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
            new_chunk = static_cast<ChunkHeader*>(system_allocator()->allocate(chunk_size_, chunk_alignment_));
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


const Type* World::entity_type_ = nullptr;


World::World(const WorldDescriptor& desc)
    : entities_(desc.entity_pool_chunk_size * sizeof(Entity)),
      archetype_allocator_(kibibytes(64), alignof(Archetype)),
      component_allocator_(kibibytes(64), alignof(ComponentChunk))
{
    if (entity_type_ == nullptr)
    {
        entity_type_ = get_type<Entity>();
    }

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

    auto archetype = lookup_or_create_archetype(&entity_type_, 1);
    auto chunk = archetype->last_chunk;
    auto offset = 0;

    for (int e = 0; e < count; ++e)
    {
        dst[e] = entities_.allocate();
        BEE_ASSERT(dst[e].is_valid());
    }

    // We allocated the entities sequentially so, aside from jumps between resource chunks, this iterator
    // should be contiguous access
    auto info_iter = entities_.get_iterator(dst[0]);

    while (chunk != nullptr)
    {
        const auto copy_count = math::min(count - offset, chunk->capacity - chunk->count);
        memcpy(chunk->data + chunk->count * sizeof(Entity), dst + offset, copy_count * sizeof(Entity));
        chunk->count += copy_count;
        offset += copy_count;

        for (int e = 0; e < copy_count; ++e)
        {
            BEE_ASSERT_F(info_iter != entities_.end(), "More default chunks were created than entities");
            info_iter->chunk = chunk;
            info_iter->index_in_chunk = e;
            ++info_iter;
        }

        if (offset >= count)
        {
            break;
        }

        if (chunk->next == nullptr)
        {
            chunk = create_chunk(archetype);
        }
        else
        {
            chunk = chunk->next;
        }
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
        auto& info = entities_[to_destroy[e]];
        destroy_entity(&info);
        entities_.deallocate(to_destroy[e]);
    }
}

ArchetypeHandle World::get_archetype(const Type* const* types, const i32 type_count)
{
    auto sorted_types = BEE_ALLOCA_ARRAY(const Type*, type_count + 1);
    sorted_types_fill(sorted_types, types, type_count);
    auto archetype = lookup_archetype(types, type_count + 1);
    return archetype == nullptr ? ArchetypeHandle{} : ArchetypeHandle { archetype->hash };
}

ArchetypeHandle World::create_archetype(const Type* const* types, const i32 type_count)
{
    auto sorted_types = BEE_ALLOCA_ARRAY(const Type*, type_count);
    sorted_types_fill(sorted_types, types, type_count);
    auto archetype = lookup_or_create_archetype(sorted_types, type_count + 1);
    BEE_ASSERT(archetype != nullptr);
    return ArchetypeHandle { archetype->hash };
}

void World::destroy_archetype(const ArchetypeHandle& archetype)
{
    auto archetype_ptr = archetype_lookup_.find(archetype.id);
    BEE_ASSERT_F(archetype_ptr != nullptr, "No archetype with the ID %u exists", archetype.id);
    destroy_archetype(archetype_ptr->value);
}

Archetype* World::lookup_archetype(const Type* const* sorted_types, const i32 type_count)
{
    const auto hash = get_archetype_hash(sorted_types, type_count);
    auto archetype = archetype_lookup_.find(hash);
    return archetype != nullptr ? archetype->value : nullptr;
}

Archetype* World::lookup_or_create_archetype(const Type* const* sorted_types, const i32 type_count)
{
    BEE_ASSERT(sorted_types[0] == get_type<Entity>());

    const auto archetype_hash = get_archetype_hash(sorted_types, type_count);
    auto mapped = archetype_lookup_.find(archetype_hash);
    if (mapped != nullptr)
    {
        return mapped->value;
    }

    const auto archetype_size = sizeof(Archetype) + type_count * (sizeof(const Type*) + sizeof(size_t*)); // NOLINT
    auto archetype_memory = static_cast<u8*>(BEE_MALLOC_ALIGNED(archetype_allocator_, archetype_size, alignof(Archetype)));
    auto archetype_types = archetype_memory + sizeof(Archetype);
    auto archetype_offsets = archetype_memory + sizeof(Archetype) + sizeof(const Type*) * type_count; // NOLINT
    auto archetype = reinterpret_cast<Archetype*>(archetype_memory);
    
    new (archetype) Archetype{};
    archetype->hash = archetype_hash;
    archetype->type_count = type_count;
    archetype->types = reinterpret_cast<const Type**>(archetype_types);
    archetype->offsets = reinterpret_cast<size_t*>(archetype_offsets);
    archetype->chunk_size = component_allocator_.max_allocation_size(); // TODO(Jacob): allow variable-sized archetypes
    archetype->chunk_count = 0;

    for (int t = 0; t < type_count; ++t)
    {
        archetype->offsets[t] = archetype->entity_size;
        archetype->types[t] = sorted_types[t];
        archetype->entity_size += sorted_types[t]->size;
    }

    create_chunk(archetype);
    archetype_lookup_.insert(archetype_hash, archetype);
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

    archetype_lookup_.erase(archetype->hash);
    destruct(archetype);
    BEE_FREE(archetype_allocator_, archetype);
}

ComponentChunk* World::create_chunk(Archetype* archetype)
{
    auto ptr = static_cast<u8*>(BEE_MALLOC_ALIGNED(component_allocator_, archetype->chunk_size, alignof(ComponentChunk)));
    auto chunk = reinterpret_cast<ComponentChunk*>(ptr);
    chunk->next = nullptr;
    chunk->previous = archetype->last_chunk;
    chunk->allocated_size = archetype->chunk_size - sizeof(ComponentChunk);
    chunk->bytes_per_entity = archetype->entity_size;
    chunk->capacity = chunk->allocated_size / chunk->bytes_per_entity;
    chunk->count = 0;
    chunk->archetype = archetype;
    chunk->data = ptr + sizeof(ComponentChunk);

    if (archetype->first_chunk == nullptr)
    {
        archetype->first_chunk = archetype->last_chunk = chunk;
    }
    else
    {
        archetype->last_chunk->next = chunk;
        archetype->last_chunk = chunk;
    }

    ++archetype->chunk_count;
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

    --chunk->archetype->chunk_count;

    BEE_FREE(component_allocator_, chunk);
}

void World::destroy_entity(EntityInfo* info)
{
    BEE_ASSERT(info->index_in_chunk < info->chunk->count);

    auto archetype = info->chunk->archetype;
    auto old_chunk = info->chunk;

    // If this is not last entity in the chunk we have to swap it with the last one in the archetype
    if (info->index_in_chunk != info->chunk->count - 1)
    {
        auto last_chunk = archetype->last_chunk;

        // Copy the last entities components and then decrement the count of that chunk
        copy_components_in_chunks(info->chunk, info->index_in_chunk, last_chunk, last_chunk->count - 1);
        old_chunk = last_chunk;

        auto last_entity_moved = reinterpret_cast<Entity*>(info->chunk->data + sizeof(Entity) * info->index_in_chunk);
        auto& last_entity_info = entities_[*last_entity_moved];
        last_entity_info.chunk = info->chunk;
        last_entity_info.index_in_chunk = info->index_in_chunk;
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
        info->index_in_chunk = -1;
        info->chunk = nullptr;
    }
}

void World::move_entity(EntityInfo* info, Archetype* dst)
{
    BEE_ASSERT(dst->first_chunk != nullptr);
    BEE_ASSERT(dst->last_chunk != nullptr);
    BEE_ASSERT(info->chunk != nullptr);

    if (info->chunk->archetype == dst)
    {
        log_warning("Tried to move an entity into the same archetype it's already in");
        return;
    }

    // Allocate a new chunk if the archetype is full
    if (dst->last_chunk->count >= dst->last_chunk->capacity)
    {
        create_chunk(dst);
    }

    // Copy the components from the old to the new chunk - last is always the only empty chunk
    copy_components_in_chunks(dst->last_chunk, dst->last_chunk->count, info->chunk, info->index_in_chunk);
    destroy_entity(info);

    ++dst->last_chunk->count;
    info->index_in_chunk = dst->last_chunk->count - 1;
    info->chunk = dst->last_chunk;
}

bool World::has_component(const EntityInfo* info, const Type* type)
{
    for (int t = 0; t < info->chunk->archetype->type_count; ++t)
    {
        if (info->chunk->archetype->types[t]->hash == type->hash)
        {
            return true;
        }
    }
    return false;
}

u32 get_archetype_hash(const Type* const* sorted_types, const i32 type_count)
{
    HashState hash(0xF00D);
    for (int t = 0; t < type_count; ++t)
    {
        hash.add(sorted_types[t]->hash);
    }
    return hash.end();
}

void sort_types(const Type** types, const i32 count)
{
    std::sort(types, types + count, [](const Type* lhs, const Type* rhs)
    {
        return lhs->hash > rhs->hash;
    });
}

i32 sorted_types_fill(const Type** dst, const Type* const* src, const i32 count)
{
    dst[0] = get_type<Entity>();
    memcpy(dst + 1, src, sizeof(const Type*) * count); // NOLINT
    sort_types(dst + 1, count);
    return count + 1;
}

i32 sorted_types_fill_append(const Type** dst, const Type* const* sorted_types, const i32 types_count, const Type* appended_type)
{
    BEE_ASSERT(appended_type != get_type<Entity>());
    BEE_ASSERT(sorted_types[0] == get_type<Entity>());
    memcpy(dst, sorted_types, sizeof(const Type*) * types_count); // NOLINT
    dst[types_count] = appended_type;
    sort_types(dst + 1, types_count);
    return types_count + 1;
}

i32 sorted_types_fill_remove(const Type** dst, const Type* const* sorted_types, const i32 types_count, const Type* removed_type)
{
    BEE_ASSERT(removed_type != get_type<Entity>());
    BEE_ASSERT(sorted_types[0] == get_type<Entity>());

    int index = 1;
    for (int t = 0; t < types_count; ++t)
    {
        if (sorted_types[t]->hash != removed_type->hash)
        {
            dst[index++] = sorted_types[t];
        }
    }

    sort_types(dst + 1, types_count - 1);
    return types_count - 1;
}

void copy_components_in_chunks(ComponentChunk* dst, const i32 dst_index, const ComponentChunk* src, const i32 src_index)
{
    BEE_ASSERT_F(src_index >= 0 && dst_index >= 0, "Invalid index given to copy_components");

    const auto src_archetype = src->archetype;
    auto dst_archetype = dst->archetype;
    auto src_type_iter = src_archetype->types;
    auto dst_type_iter = dst_archetype->types;
    auto src_type_end = src_archetype->types + src_archetype->type_count;
    auto dst_type_end = dst_archetype->types + dst_archetype->type_count;
    auto src_offset_iter = src_archetype->offsets;
    auto dst_offset_iter = dst_archetype->offsets;
    void* dst_component = nullptr;
    void* src_component = nullptr;

    /*
     * We can take advantage of the fact that the type arrays are always sorted by hash and
     * keep stepping through, only incrementing each iterator when one has a smaller hash than the other
     */
    while (src_type_iter != src_type_end && dst_type_iter != dst_type_end)
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