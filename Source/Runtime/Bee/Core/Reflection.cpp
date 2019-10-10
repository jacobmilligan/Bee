/*
 *  Reflection.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/Containers/HashMap.hpp"

namespace bee {


struct RegisteredType
{
    Type    type;
    String  name;
    String  fully_qualified_name;
    String  annotated_name;

    RegisteredType() = default;

    explicit RegisteredType(const Type& new_type)
        : type(new_type),
          name(new_type.name),
          fully_qualified_name(new_type.fully_qualified_name),
          annotated_name(new_type.annotated_name)
    {
        type.name = name.c_str();
        type.fully_qualified_name = fully_qualified_name.c_str();
        type.annotated_name = annotated_name.c_str();
    }
};

static TypeRegistrationListNode* first_type = nullptr;
static TypeRegistrationListNode* last_type = nullptr;

static DynamicHashMap<u32, RegisteredType> type_map;

TypeRegistrationListNode::TypeRegistrationListNode(const Type& new_type)
    : type(new_type),
      next(nullptr)
{
    if (first_type == nullptr)
    {
        first_type = last_type = this;
    }
    else
    {
        last_type->next = this;
        last_type = last_type->next;
    }
}

void reflection_init()
{
    TypeRegistrationListNode* registration = first_type;

    while (registration != nullptr)
    {
        register_type(registration->type);
        registration = registration->next;
    }

    // Register builtin types
    register_type<bool>();
    register_type<i8>();
    register_type<i16>();
    register_type<i32>();
    register_type<i64>();
    register_type<u8>();
    register_type<u16>();
    register_type<u32>();
    register_type<u64>();
    register_type<float>();
    register_type<double>();
}

void register_type(const Type& type)
{
    BEE_ASSERT_F(type_map.find(type.hash) == nullptr, "Reflected type `%s` was registered multiple times", type.name);
    type_map.insert(type.hash, RegisteredType(type));
}

void unregister_type(const u32 hash)
{
    type_map.erase(hash);
}

Type get_type(const u32 hash) noexcept
{
    auto registered_type = type_map.find(hash);
    return registered_type == nullptr ? Type{} : registered_type->value.type;
}


} // namespace bee