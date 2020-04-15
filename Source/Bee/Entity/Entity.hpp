/*
 *  World.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Entity/Component.hpp"

#include "Bee/Core/Containers/ResourcePool.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Memory/PoolAllocator.hpp"
#include "Bee/Core/Memory/SmartPointers.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Concurrency.hpp"


namespace bee {


BEE_VERSIONED_HANDLE_64(Entity) BEE_REFLECT(serializable);
BEE_RAW_HANDLE_U32(EntityComponentQuery) BEE_REFLECT(serializable);

class World;

struct EntityComponentQueryData
{
    u32                 hash { 0 };
    i32                 type_count { 0 };
    const Type**        types;
    Span<const Type*>   write_types;
    Span<const Type*>   read_types;
};

struct EntityComponentQueryTypeInfo
{
    bool        read_only { false };
    const Type* type { nullptr };
};

template <typename ActualJobType>
struct EntitySystemJob : public Job
{
public:
    void init(World* owning_world, const EntityComponentQuery& query_to_run);

    void execute() final;
private:
    World*                      world_ { nullptr };
    EntityComponentQuery        query_;

    void execute_for_each_in_chunk(ComponentChunk* chunk);
};

class EntitySystem
{
public:
    virtual void init() { };

    virtual void execute() = 0;

    void init_with_world(World* world)
    {
        world_ = world;
        init();
    }

    template <typename T>
    EntityComponentQueryTypeInfo read()
    {
        return EntityComponentQueryTypeInfo { true, get_type<T>() };
    }

    template <typename T>
    EntityComponentQueryTypeInfo read_write()
    {
        return EntityComponentQueryTypeInfo { false, get_type<T>() };
    }

    template <typename... Infos>
    EntityComponentQuery get_or_create_query(Infos&&... infos)
    {
        EntityComponentQueryTypeInfo query_infos[sizeof...(Infos)] { infos... };
        return world_->get_or_create_query(query_infos, sizeof...(Infos));
    }

    template <typename CallbackType>
    inline void for_each_entity(const EntityComponentQuery& query, CallbackType&& callback);

    template <typename JobType, typename... ConstructorArgs>
    void execute_jobs(const EntityComponentQuery& query, JobGroup* group, ConstructorArgs&&... args);
private:
    World* world_ { nullptr };
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
    inline ArchetypeHandle create_archetype(); // not thread-safe

    template <typename... Types>
    inline ArchetypeHandle get_archetype(); // not thread-safe

    ArchetypeHandle get_archetype(const Type* const* types, const i32 type_count); // not thread-safe

    ArchetypeHandle create_archetype(const Type* const* types, const i32 type_count); // not thread-safe

    void destroy_archetype(const ArchetypeHandle& archetype); // not thread-safe

    /*
     * Entity management
     */
    Entity create_entity(); // not thread-safe

    Entity create_entity(const ArchetypeHandle& archetype);  // not thread-safe

    void create_entities(Entity* dst, const i32 count); // not thread-safe

    void create_entities(const ArchetypeHandle& archetype, Entity* dst, const i32 count); // not thread-safe

    void destroy_entity(const Entity& entity); // not thread-safe

    void destroy_entities(const Entity* to_destroy, const i32 count); // not thread-safe

    template <typename T, typename... Args>
    inline T* add_component(const Entity& entity, Args&&... args); // not thread-safe

    template <typename T>
    inline void remove_component(const Entity& entity); // not thread-safe

    template <typename T>
    inline bool has_component(const Entity& entity) const; // not thread-safe

    template <typename T>
    inline T* get_component(const Entity& entity); // not thread-safe

    /*
     * Query management
     */
    EntityComponentQuery get_or_create_query(const EntityComponentQueryTypeInfo* type_infos, const i32 count);

    void query_chunks(const EntityComponentQuery& handle, DynamicArray<ComponentChunk*>* results);

    void query_chunks(JobGroup* wait_handle, const EntityComponentQuery& query_handle, DynamicArray<ComponentChunk*>* results);

    /*
     * System management
     */
    template <typename T, typename... Args>
    inline void add_system(Args&&... args) noexcept; // not thread-safe

    template <typename T>
    inline T* get_system() noexcept;

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

    static const Type*                      entity_type_;

    ResourcePool<Entity, EntityInfo>        entities_;

    /*
     * Component management
     */
    ChunkAllocator                          component_allocator_;
    ChunkAllocator                          archetype_allocator_;
    DynamicHashMap<u32, Archetype*>         archetype_lookup_;

    /*
     * System management
     */
    struct SystemInfo
    {
        const Type*     type { nullptr };
        EntitySystem*   instance { nullptr };
    };

    DynamicArray<UniquePtr<EntitySystem>>           systems_;
    DynamicArray<DynamicArray<EntitySystem*>>       system_groups_;
    DynamicHashMap<u32, SystemInfo>                 system_lookup_;

    /*
     * Query management
     */
    ChunkAllocator                                  query_allocator_;
    DynamicHashMap<u32, EntityComponentQueryData*>  queries_;
    EntityComponentDependencyMap                    dependencies_;

    /*
     * Implementation
     */
    Archetype* lookup_archetype(const Type* const* sorted_types, const i32 type_count);

    Archetype* get_or_create_archetype(const Type* const* sorted_types, const i32 type_count);

    void destroy_archetype(Archetype* archetype);

    ComponentChunk* create_chunk(Archetype* archetype);

    void destroy_chunk(ComponentChunk* chunk);

    void create_entities(Archetype* archetype, Entity* dst, const i32 count);

    void move_entity(EntityInfo* info, Archetype* dst);

    void destroy_entity(EntityInfo* info);

    static bool has_component(const EntityInfo* info, const Type* type);

    static void* get_component_ptr(EntityInfo* info, const Type* type);
};

BEE_RUNTIME_API u32 get_archetype_hash(const Type* const* sorted_types, const i32 type_count);

BEE_RUNTIME_API void copy_components_in_chunks(ComponentChunk* dst, const i32 dst_index, const ComponentChunk* src, const i32 src_index);

BEE_RUNTIME_API void sort_types(const Type** types, const i32 count);

BEE_RUNTIME_API i32 sorted_types_fill(const Type** dst, const Type* const* src, const i32 count);

BEE_RUNTIME_API i32 sorted_types_fill_append(const Type** dst, const Type* const* sorted_types, const i32 types_count, const Type* appended_type);

BEE_RUNTIME_API i32 sorted_types_fill_remove(const Type** dst, const Type* const* sorted_types, const i32 types_count, const Type* removed_type);


} // namespace bee

#include "Bee/Entity/Entity.inl"