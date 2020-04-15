/*
 *  Entity.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Entity/Entity.hpp"

#include <algorithm>


namespace bee {


const Type* World::entity_type_ = nullptr;


World::World(const WorldDescriptor& desc)
    : entities_(desc.entity_pool_chunk_size * sizeof(Entity)),
      archetype_allocator_(kibibytes(64), alignof(Archetype)),
      component_allocator_(kibibytes(64), alignof(ComponentChunk)),
      query_allocator_(kibibytes(64), alignof(EntityComponentQueryData))
{
    BEE_ASSERT_F(get_job_worker_count() >= 1, "Job system must be initialized before creating a World");

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

Entity World::create_entity(const ArchetypeHandle &archetype)
{
    Entity entity{};
    create_entities(archetype, &entity, 1);
    return entity;
}

void World::create_entities(const ArchetypeHandle& archetype, Entity* dst, const i32 count)
{
    auto stored_archetype = archetype_lookup_.find(archetype.id);
    if (BEE_FAIL_F(stored_archetype != nullptr, "Invalid archetype handle"))
    {
        return;
    }

    create_entities(stored_archetype->value, dst, count);
}

void World::create_entities(Entity* dst, const i32 count)
{
    auto archetype = get_or_create_archetype(&entity_type_, 1);
    create_entities(archetype, dst, count);
}

void World::create_entities(Archetype* archetype, Entity* dst, const i32 count)
{
    BEE_ASSERT(dst != nullptr);
    BEE_ASSERT(count > 0);

    auto chunk = archetype->last_chunk;
    auto offset = 0;

    for (int e = 0; e < count; ++e)
    {
        dst[e] = entities_.allocate();
        BEE_ASSERT(dst[e].is_valid());
        BEE_ASSERT(dst[e].version() > 0);
    }

    // We allocated the entities sequentially so, aside from jumps between resource chunks, this iterator
    // should be contiguous access
    auto info_iter = entities_.get_iterator(dst[0]);

    while (chunk != nullptr)
    {
        const auto copy_count = math::min(count - offset, chunk->capacity - chunk->count);
        const auto remaining = chunk->capacity - chunk->count - copy_count;
        memcpy(chunk->data + chunk->count * sizeof(Entity), dst + offset, copy_count * sizeof(Entity));

        // Initialize remaining entities in chunk to an invalid ID for debugging
        for (int i = 0; i < remaining; ++i)
        {
            reinterpret_cast<Entity*>(chunk->data)[chunk->count + copy_count + i].id = Entity::generator_t::invalid_id;
        }

        // Initialize all the components to zero
        for (int t = 1; t < archetype->type_count; ++t)
        {
            const auto component_offset = archetype->offsets[t] + chunk->count * archetype->types[t]->size;
            memset(chunk->data + component_offset, 0, archetype->types[t]->size * copy_count);
        }

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
    auto archetype = lookup_archetype(sorted_types, type_count + 1);
    return archetype == nullptr ? ArchetypeHandle{} : ArchetypeHandle { archetype->hash };
}

ArchetypeHandle World::create_archetype(const Type* const* types, const i32 type_count)
{
    auto sorted_types = BEE_ALLOCA_ARRAY(const Type*, type_count);
    sorted_types_fill(sorted_types, types, type_count);
    auto archetype = get_or_create_archetype(sorted_types, type_count + 1);
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

Archetype* World::get_or_create_archetype(const Type* const* sorted_types, const i32 type_count)
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
        archetype->types[t] = sorted_types[t];
        archetype->entity_size += sorted_types[t]->size;

        dependencies_.add_type_if_not_registered(sorted_types[t]);
    }

    auto first_chunk = create_chunk(archetype);

    int offset = 0;
    for (int t = 0; t < type_count; ++t)
    {
        archetype->offsets[t] = offset;
        offset += sign_cast<i32>(sorted_types[t]->size) * first_chunk->capacity;
    }

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
    new (chunk) ComponentChunk{};

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
    if (old_chunk->count > 1)
    {
        auto last_chunk = archetype->last_chunk;

        BEE_ASSERT(reinterpret_cast<Entity*>(last_chunk->data + sizeof(Entity) * (last_chunk->count - 1))->version() > 0);

        // Copy the last entities components and then decrement the count of that chunk
        copy_components_in_chunks(old_chunk, info->index_in_chunk, last_chunk, last_chunk->count - 1);

        auto last_entity_moved = reinterpret_cast<Entity*>(old_chunk->data + sizeof(Entity) * info->index_in_chunk);
        auto& last_entity_info = entities_[*last_entity_moved];
        last_entity_info.chunk = old_chunk;
        last_entity_info.index_in_chunk = info->index_in_chunk;
        old_chunk = last_chunk;
    }

    --old_chunk->count;
    info->index_in_chunk = -1;
    info->chunk = nullptr;

    if (old_chunk->count <= 0)
    {
        destroy_chunk(old_chunk);
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

void* World::get_component_ptr(EntityInfo* info, const Type* type)
{
    const auto type_count = info->chunk->archetype->type_count;
    auto& offsets = info->chunk->archetype->offsets;
    auto& types = info->chunk->archetype->types;
    u8* ptr = nullptr;

    // Search the archetype for the type
    for (int t = 0; t < type_count; ++t)
    {
        if (types[t] == type)
        {
            ptr = info->chunk->data + offsets[t] + type->size * info->index_in_chunk;
            break;
        }
    }

    if (BEE_FAIL_F(ptr != nullptr, "Entity does not have component %s", type->name))
    {
        return nullptr;
    }

    return ptr;
}

void sort_type_infos(EntityComponentQueryTypeInfo* infos, const i32 count)
{
    std::sort(infos, infos + count, [](const EntityComponentQueryTypeInfo& lhs, const EntityComponentQueryTypeInfo& rhs)
    {
        if (lhs.read_only != rhs.read_only)
        {
            return lhs.read_only < rhs.read_only;
        }
        return lhs.type->hash < rhs.type->hash;
    });
}

EntityComponentQuery World::get_or_create_query(const EntityComponentQueryTypeInfo* type_infos, const i32 count)
{
    auto sorted_type_infos = BEE_ALLOCA_ARRAY(EntityComponentQueryTypeInfo, count);
    memcpy(sorted_type_infos, type_infos, sizeof(EntityComponentQueryTypeInfo) * count);
    sort_type_infos(sorted_type_infos, count);

    HashState hash_state;
    for (int t = 0; t < count; ++t)
    {
        hash_state.add(sorted_type_infos[t].read_only);
        hash_state.add(sorted_type_infos[t].type->hash);
    }
    const auto hash = hash_state.end();

    if (queries_.find(hash) == nullptr)
    {
        auto query_mem = BEE_MALLOC(query_allocator_, sizeof(EntityComponentQueryData) + sizeof(const Type* const) * count);
        auto query = static_cast<EntityComponentQueryData*>(query_mem);
        query->hash = hash;
        query->type_count = count;
        query->types = reinterpret_cast<const Type**>(static_cast<u8*>(query_mem) + sizeof(EntityComponentQueryData));

        int read_only_begin = -1;
        for (int i = 0; i < count; ++i)
        {
            if (sorted_type_infos[i].read_only && read_only_begin < 0)
            {
                read_only_begin = i;
            }

            query->types[i] = sorted_type_infos[i].type;
        }

        query->write_types = make_span<const Type*>(query->types, read_only_begin);
        query->read_types = make_span<const Type*>(query->types + read_only_begin, count - read_only_begin);
        queries_.insert(hash, query);
    }

    return EntityComponentQuery { hash };
}

bool is_valid_archetype(const Archetype& archetype, const EntityComponentQueryData& query)
{
    // archetype types and query types are both sorted
    auto query_type = query.types;
    auto query_types_end = query.types + query.type_count;
    auto archetype_type = archetype.types;
    auto archetype_types_end = archetype.types + archetype.type_count;

    while (query_type != query_types_end && archetype_type != archetype_types_end)
    {
        const auto query_type_hash = (*query_type)->hash;
        const auto archetype_type_hash = (*archetype_type)->hash;

        if (query_type_hash == archetype_type_hash)
        {
            // types are the same
            ++query_type;
            ++archetype_type;
        }
        else if (query_type_hash < archetype_type_hash)
        {
            // query type is lower in array than archetype type
            ++query_type;
        }
        else
        {
            // archetype type is lower in array than query type
            ++archetype_type;
        }
    }

    return query_type == query_types_end;
}

struct GetChunksForQueryJob final : public Job
{
    const DynamicHashMap<u32, Archetype*>*  archetypes { nullptr };
    DynamicArray<ComponentChunk*>*          results { nullptr };
    EntityComponentQueryData*               query { nullptr };

    explicit GetChunksForQueryJob(EntityComponentQueryData* query_in_use, const DynamicHashMap<u32, Archetype*>* valid_archetypes, DynamicArray<ComponentChunk*>* results_array)
        : query(query_in_use),
          archetypes(valid_archetypes),
          results(results_array)
    {}

    void execute() override
    {
        for (auto archetype : *archetypes)
        {
            if (!is_valid_archetype(*archetype.value, *query))
            {
                continue;
            }

            auto chunk = archetype.value->first_chunk;
            while (chunk != nullptr)
            {
                results->push_back(chunk);
                chunk = chunk->next;
            }
        }
    }
};

// TODO(Jacob): cache this result and use dirty flag to only gather chunks when needed
void World::query_chunks(const EntityComponentQuery& handle, DynamicArray<ComponentChunk*>* results)
{
    JobGroup group;
    query_chunks(&group, handle, results);
    job_wait(&group);
}

void World::query_chunks(JobGroup* wait_handle, const EntityComponentQuery& query_handle, DynamicArray<ComponentChunk*>* results)
{
    auto cached_query = queries_.find(query_handle.id);
    if (cached_query == nullptr)
    {
        return;
    }

    auto query = cached_query->value;
    auto get_chunks_job = allocate_job<GetChunksForQueryJob>(query, &archetype_lookup_, results);

    dependencies_.add_dependencies(EntityComponentAccess::read_only, wait_handle, query->read_types, query->write_types);

    job_schedule(wait_handle, get_chunks_job);
}

u32 get_archetype_hash(const Type* const* sorted_types, const i32 type_count)
{
    HashState hash;
    for (int t = 0; t < type_count; ++t)
    {
        hash.add(sorted_types[t]->hash);
    }
    return hash.end();
}

void sort_types(const Type** types, const i32 count)
{
    static const Type* entity_type = get_type<Entity>();

    auto begin = types;
    auto end = types + count;

    if (types[0] == entity_type)
    {
        if (count <= 1)
        {
            return;
        }

        ++begin;
    }

    std::sort(begin, end, [](const Type* lhs, const Type* rhs)
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