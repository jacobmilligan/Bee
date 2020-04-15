/*
 *  Entity.inl
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection.hpp"

#include <type_traits>


namespace bee {


template <typename T, typename... Args>
inline T* World::add_component(const Entity& entity, Args&& ... args)
{
    static_assert(std::is_trivially_copyable<T>::value, "Component type T must be trivially copyable");

    auto& info = entities_[entity];
    auto type = get_type<T>();

    Archetype* new_archetype = nullptr;

    // chunk will be null if the entity has no components - won't need to be moved from the old archetype
    if (info.chunk == nullptr)
    {
        BEE_UNREACHABLE("entity should at least be in the default `Entity` archetype");
    }
    else
    {
        auto old_archetype = info.chunk->archetype;
        auto types = BEE_ALLOCA_ARRAY(const Type*, old_archetype->type_count + 1);
        const auto new_type_count = sorted_types_fill_append(types, old_archetype->types, old_archetype->type_count, type);
        new_archetype = get_or_create_archetype(types, new_type_count);

        if (new_archetype == old_archetype)
        {
            log_warning("Entity %" PRIu64 " already has an instance of component %s", entity.id, type->name);
            return static_cast<T*>(get_component_ptr(&info, type));
        }
    }

    move_entity(&info, new_archetype);

    auto component = static_cast<T*>(get_component_ptr(&info, type));
    new (component) T { std::forward<Args>(args)... };

    return component;
}

template <typename T>
inline void World::remove_component(const Entity& entity)
{
    auto type = get_type<T>();
    auto& info = entities_[entity];
    auto old_archetype = info.chunk->archetype;
    const auto old_type_count = old_archetype->type_count;

    // check if we only have the entity type in the array
    if (old_type_count - 1 <= 1)
    {
        destroy_entity(&info); // this will also destroy any archetypes or chunks
    }
    else
    {
        auto types = BEE_ALLOCA_ARRAY(const Type*, old_type_count - 1);
        const auto new_type_count = sorted_types_fill_remove(types, old_archetype->types, old_type_count, type);
        auto new_archetype = get_or_create_archetype(types, new_type_count);
        move_entity(&info, new_archetype);
    }
}

template <typename T>
inline bool World::has_component(const Entity& entity) const
{
    return has_component(entity, get_type<T>());
}

template <typename T>
inline T* World::get_component(const Entity& entity)
{
    auto& info = entities_[entity];
    return static_cast<T*>(get_component_ptr(&info, get_type<T>()));
}

template <typename... Types>
inline ArchetypeHandle World::create_archetype()
{
    const Type* types[] = { get_type<Entity>(), get_type<Types>()... };
    sort_types(types, static_array_length(types));
    auto archetype = get_or_create_archetype(types, static_array_length(types));
    return ArchetypeHandle { archetype->hash };
}

template <typename... Types>
inline ArchetypeHandle World::get_archetype()
{
    const Type* const types[] = { get_type<Types>()... };
    return get_archetype(types, sizeof...(Types));
}

template <typename T, typename... Args>
inline void World::add_system(Args&&... args) noexcept
{
    static_assert(std::is_base_of<EntitySystem, T>::value, "T must derive from EntitySystem");

    auto system_type = get_type<T>();
    if (system_lookup_.find(system_type->hash) != nullptr)
    {
        log_warning("EntitySystem %s is already registered for that world", system_type->name);
        return;
    }

    auto system = BEE_NEW(system_allocator(), T)(std::forward<Args>(args)...);
    system->init_with_world(this);
    systems_.emplace_back(system, system_allocator());
    system_lookup_.insert(system_type->hash, { system_type, systems_.back().get() });
}

template <typename T>
inline T* World::get_system() noexcept
{
    static_assert(std::is_base_of<EntitySystem, T>::value, "T must derive from EntitySystem");

    const auto system_type = get_type<T>();
    auto system = system_lookup_.find(system_type->hash);
    if (BEE_FAIL_F(system != nullptr, "Couldn't find EntitySystem %s", system_type->name))
    {
        return nullptr;
    }
    return reinterpret_cast<T*>(system->value.instance);
}

template <typename T>
inline typename remove_cv_ref_ptr<T>::type* get_component_array(ComponentChunk* chunk)
{
    // remove all cv-qualifiers and any reference or pointer from T
    using actual_t = remove_cv_ref_ptr<T>::type;

    const auto type = get_type<actual_t>();

    // Search the archetype for the type
    for (int t = 0; t < chunk->archetype->type_count; ++t)
    {
        if (chunk->archetype->types[t] == type)
        {
            return reinterpret_cast<actual_t*>(chunk->data + chunk->archetype->offsets[t]);
        }
    }

    return nullptr;
}

template <typename CallbackType, typename... Args>
inline void for_each_in_chunk(ComponentChunk* chunk, CallbackType&& callback, function_args_list<Args...> /*args_list*/)
{
    auto args_tuple = std::make_tuple(get_component_array<Args>(chunk)...);

    for (int e = 0; e < chunk->count; ++e)
    {
        callback(std::get<decltype(get_component_array<Args>(chunk))>(args_tuple)[e]...);
    }
}

template <typename ClassType, typename... Args>
inline void for_each_in_chunk(ComponentChunk* chunk, ClassType* cls, void(ClassType::*callback)(const i32, Args...))
{
    auto args_tuple = std::make_tuple(get_component_array<Args>(chunk)...);

    (cls->*callback)(chunk->count, std::get<decltype(get_component_array<Args>(chunk))>(args_tuple)...);
}

template <typename CallbackType>
inline void EntitySystem::for_each_entity(const EntityComponentQuery &query, CallbackType&& callback)
{
    // TODO(Jacob): validate query and callback have matching types

    DynamicArray<ComponentChunk*> chunks(temp_allocator());
    world_->query_chunks(query, &chunks);

    for (auto chunk : chunks)
    {
        for_each_in_chunk(chunk, callback, function_traits<CallbackType>::args_list_t{});
    }
}

template <typename ActualJobType>
void EntitySystemJob<ActualJobType>::init(World* owning_world, const EntityComponentQuery& query_to_run)
{
    world_ = owning_world;
    query_ = query_to_run;
}

template <typename ActualJobType>
void EntitySystemJob<ActualJobType>::execute()
{
    DynamicArray<ComponentChunk*> chunks(job_temp_allocator());

    JobGroup group;
    world_->query_chunks(&group, query_, &chunks);
    job_wait(&group);

    for (int i = 0; i < chunks.size(); i++)
    {
        job_schedule(&group, allocate_job([&, chunk_index=i]()
        {
            this->execute_for_each_in_chunk(chunks[chunk_index]);
        }));
    }

    job_wait(&group);
}

template <typename ActualJobType>
void EntitySystemJob<ActualJobType>::execute_for_each_in_chunk(ComponentChunk* chunk)
{
    for_each_in_chunk(chunk, static_cast<ActualJobType*>(this), &ActualJobType::for_each);
}

template <typename JobType, typename... ConstructorArgs>
void EntitySystem::execute_jobs(const EntityComponentQuery& query, JobGroup* group, ConstructorArgs&&... args)
{
    static_assert(std::is_base_of<EntitySystemJob<JobType>, JobType>::value, "JobType must derive from EntitySystemJob<JobType>");
    auto job = allocate_job<JobType>(std::forward<ConstructorArgs>(args)...);
    job->init(world_, query);
    job_schedule(group, job);
}


} // namespace bee