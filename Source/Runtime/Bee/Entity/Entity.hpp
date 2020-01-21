/*
 *  World.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/ResourcePool.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Memory/PoolAllocator.hpp"


namespace bee {


struct Type;


BEE_VERSIONED_HANDLE_64(Entity) BEE_REFLECT(serializable);
BEE_RAW_HANDLE_U32(ArchetypeHandle);

struct Archetype;

struct ComponentChunk
{
    ComponentChunk* next { nullptr };
    ComponentChunk* previous { nullptr };

    size_t          allocated_size { 0 };
    size_t          bytes_per_entity { 0 };
    i32             capacity { 0 };
    i32             count { 0 };
    Archetype*      archetype { nullptr };
    u8*             data { nullptr };
};


struct Archetype
{
    u32             hash { 0 };
    size_t          chunk_size { 0 };
    size_t          entity_size { 0 };
    i32             type_count { 0 };
    const Type**    types { nullptr };
    size_t*         offsets { nullptr };
    i32             chunk_count { 0 };
    ComponentChunk* first_chunk { nullptr };
    ComponentChunk* last_chunk { nullptr };
};


class ChunkAllocator final : public Allocator
{
public:
    ChunkAllocator() = default;

    ChunkAllocator(const size_t chunk_size, const size_t chunk_alignment);

    ~ChunkAllocator() override;

    bool is_valid(const void* ptr) const override;

    void* allocate(size_t size, size_t alignment) override;

    void* reallocate(void* ptr, size_t old_size, size_t new_size, size_t alignment) override;

    void deallocate(void* ptr) override;

    inline size_t max_allocation_size() const
    {
        return chunk_size_ - sizeof(AllocHeader) - sizeof(ChunkHeader);
    }
private:
    static constexpr u32 header_signature = 0x23464829;

    struct ChunkHeader
    {
        ChunkHeader*    next { nullptr };
        u32             signature { header_signature };
        u8*             data { nullptr };
        size_t          size { 0 };
    };

    struct AllocHeader
    {
        ChunkHeader*    chunk { nullptr };
        size_t          size { 0 };
    };

    size_t          chunk_size_ { 0 };
    size_t          chunk_alignment_ { 0 };
    ChunkHeader*    first_ { nullptr };
    ChunkHeader*    last_ { nullptr };
    ChunkHeader*    free_ { nullptr };

    static AllocHeader* get_alloc_header(void* ptr);

    static const AllocHeader* get_alloc_header(const void* ptr);
};


struct WorldDescriptor
{
    i32 entity_pool_chunk_size { 1024 };
};

class BEE_RUNTIME_API World
{
public:
    World() = default;

    explicit World(const WorldDescriptor& desc);

    /*
     * Archetype management
     */
    template <typename... Types>
    ArchetypeHandle get_or_create_archetype();

    template <typename... Types>
    ArchetypeHandle get_archetype();

    ArchetypeHandle get_archetype(const Type* const* types, const i32 type_count);

    ArchetypeHandle create_archetype(const Type* const* types, const i32 type_count);

    void destroy_archetype(const ArchetypeHandle& archetype);

    /*
     * Entity management
     */
    Entity create_entity(); // not thread-safe

    void create_entities(Entity* dst, const i32 count); // not thread-safe

    void destroy_entity(const Entity& entity); // not thread-safe

    void destroy_entities(const Entity* to_destroy, const i32 count); // not thread-safe

    template <typename T, typename... Args>
    T* add_component(const Entity& entity, Args&&... args); // not thread-safe

    template <typename T>
    void remove_component(const Entity& entity); // not thread-safe

    template <typename T>
    bool has_component(const Entity& entity) const; // not thread-safe

    inline i64 alive_count() const
    {
        return entities_.size();
    }

    inline i32 archetype_count() const
    {
        return archetype_lookup_.size();
    }

private:
    /*
     * Entity management
     */
    struct EntityInfo
    {
        i32             index_in_chunk { 0 };
        ComponentChunk* chunk { nullptr };
    };

    static const Type* entity_type_;

    ResourcePool<Entity, EntityInfo>    entities_;

    /*
     * Component management
     */
    ChunkAllocator                      component_allocator_;
    ChunkAllocator                      archetype_allocator_;
    DynamicHashMap<u32, Archetype*>     archetype_lookup_;

    /*
     * Implementation
     */
    Archetype* lookup_archetype(const Type* const* sorted_types, const i32 type_count);

    Archetype* lookup_or_create_archetype(const Type* const* sorted_types, const i32 type_count);

    void destroy_archetype(Archetype* archetype);

    ComponentChunk* create_chunk(Archetype* archetype);

    void destroy_chunk(ComponentChunk* chunk);

    void move_entity(EntityInfo* info, Archetype* dst);

    void destroy_entity(EntityInfo* info);

    static bool has_component(const EntityInfo* info, const Type* type);

    template <typename T>
    static T* get_component_ptr(EntityInfo* info, const Type* type);
};

BEE_RUNTIME_API u32 get_archetype_hash(const Type* const* sorted_types, const i32 type_count);

BEE_RUNTIME_API void copy_components_in_chunks(ComponentChunk* dst, const i32 dst_index, const ComponentChunk* src, const i32 src_index);

BEE_RUNTIME_API void sort_types(const Type** types, const i32 count);

BEE_RUNTIME_API i32 sorted_types_fill(const Type** dst, const Type* const* src, const i32 count);

BEE_RUNTIME_API i32 sorted_types_fill_append(const Type** dst, const Type* const* sorted_types, const i32 types_count, const Type* appended_type);

BEE_RUNTIME_API i32 sorted_types_fill_remove(const Type** dst, const Type* const* sorted_types, const i32 types_count, const Type* removed_type);


} // namespace bee

#include "Bee/Entity/Entity.inl"