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


template <typename T>
inline T* World::get_component_ptr(EntityInfo* info, const Type* type)
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
        }
    }

    if (BEE_FAIL_F(ptr != nullptr, "Entity does not have component %s", type->name))
    {
        return nullptr;
    }

    return reinterpret_cast<T*>(ptr);
}


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
        new_archetype = lookup_or_create_archetype(types, new_type_count);

        if (new_archetype == old_archetype)
        {
            log_warning("Entity %" PRIu64 " already has an instance of component %s", entity.id, type->name);
            return get_component_ptr<T>(&info, type);
        }
    }

    move_entity(&info, new_archetype);

    auto component = get_component_ptr<T>(&info, type);
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
        auto new_archetype = lookup_or_create_archetype(types, new_type_count);
        move_entity(&info, new_archetype);
    }
}

template <typename T>
inline bool World::has_component(const Entity& entity) const
{
    return has_component(entity, get_type<T>());
}

template <typename... Types>
inline ArchetypeHandle World::get_or_create_archetype()
{
    const Type* const types[] = { get_type<Entity>(), get_type<Types>()... };
    sort_types(types, static_array_length(types));
    auto archetype = lookup_or_create_archetype(types, static_array_length(types));
    return ArchetypeHandle { archetype->hash };
}

template <typename... Types>
inline ArchetypeHandle World::get_archetype()
{
    const Type* const types[] = { get_type<Types>()... };
    return get_archetype(types, sizeof...(Types));
}


} // namespace bee