/*
 *  ReflectionV2.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/ReflectionV2.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Containers/HashMap.hpp"

namespace bee {


template <typename T>
constexpr size_t alignof_helper()
{
    return alignof(T);
}

template <>
constexpr size_t alignof_helper<void>()
{
    return 0;
}

template <typename T>
constexpr size_t sizeof_helper()
{
    return sizeof(T);
}

template <>
constexpr size_t sizeof_helper<void>()
{
    return 0;
}


/*
 * Register all builtin fundamentals - these are registered as regular types
 */
#define BUILTIN_TYPES               \
    BUILTIN(bool)                   \
    BUILTIN(char)                   \
    BUILTIN(unsigned char)          \
    BUILTIN(short)                  \
    BUILTIN(unsigned short)         \
    BUILTIN(int)                    \
    BUILTIN(unsigned int)           \
    BUILTIN(long)                   \
    BUILTIN(unsigned long)          \
    BUILTIN(long long)              \
    BUILTIN(unsigned long long)     \
    BUILTIN(float)                  \
    BUILTIN(double)                 \
    BUILTIN(void)

#define BUILTIN(builtin_type) \
    template <> BEE_CORE_API  constexpr u32 get_type_hash<builtin_type>() { return get_static_string_hash(#builtin_type); } \
                                                                        \
    template <> BEE_CORE_API  const Type* get_type<builtin_type>()      \
    {                                                                   \
        static constexpr Type instance                                  \
        {                                                               \
            get_type_hash<builtin_type>(),                              \
            sizeof_helper<builtin_type>(),                              \
            alignof_helper<builtin_type>(),                             \
            TypeKind::fundamental,                                      \
            #builtin_type                                               \
        };                                                              \
        return &instance;                                               \
    }

BUILTIN_TYPES

#undef BUILTIN


static DynamicHashMap<u32, const Type*> g_type_map;


void reflection_initv2()
{
    #define BUILTIN(builtin_type) get_type<builtin_type>(),

    static const Type* builtin_types[] { BUILTIN_TYPES };

    for (auto& type : builtin_types)
    {
        g_type_map.insert(type->hash, type);
    }
}


void reflection_register_type(const Type& type)
{

}

const Type* get_type(const u32 hash)
{
    static constexpr Type unknown_type { 0, 0, 0, TypeKind::unknown, "missing" };

    auto type = g_type_map.find(hash);
    if (type != nullptr)
    {
        return type->value;
    }

    return &unknown_type;
}


} // namespace bee