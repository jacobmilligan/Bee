/*
 *  Entity.inl
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection.hpp"

namespace bee {


template <typename T>
T* World::get_component_ptr(EntityInfo* info, const Type* type)
{
    auto& offsets = info->chunk->archetype->offsets;
    auto& types = info->chunk->archetype->types;
    u8* ptr = nullptr;

    // Search the archetype for the type
    for (int t = 0; t < offsets.size(); ++t)
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
T* World::add_component(const Entity& entity, Args&& ... args)
{
    static_assert(std::is_trivially_copyable<T>::value, "Component type T must be trivially copyable");

    auto& info = entities_[entity];
    auto type = get_type<T>();
    auto old_archetype = info.chunk->archetype;
    auto type_array = get_sorted_type_array_additive(old_archetype->types.const_span(), type);
    auto new_archetype = get_or_create_archetype(type_array);

    if (new_archetype == old_archetype)
    {
        log_warning("Entity %" PRIu64 " already has an instance of component %s", entity.id, type->name);
        return get_component_ptr<T>(&info, type);
    }

    move_entity(&info, new_archetype);

    auto component = get_component_ptr<T>(&info, type);
    new (component) T(std::forward<Args...>(args)...);

    return component;
}

template <typename T>
void World::remove_component(const Entity& entity)
{
    auto& info = entities_[entity];
    auto type = get_type<T>();
    auto old_archetype = info.chunk->archetype;
    auto type_array = get_sorted_type_array_subtractive(old_archetype->types.const_span(), type);

    // check if we only have the entity type in the array
    if (type_array.size() == 1)
    {
        destroy_entity(&info); // this will also destroy any archetypes or chunks
    }
    else
    {
        auto new_archetype = get_or_create_archetype(type_array);
        move_entity(&info, new_archetype);
    }
}

template <typename T>
bool World::has_component(const Entity& entity) const
{
    return has_component(entity, get_type<T>());
}


} // namespace bee