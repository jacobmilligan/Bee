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
        static Type instance                                            \
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


const Type* get_type(const u32 hash)
{
    static Type unknown_type { 0, 0, 0, TypeKind::unknown, "missing" };

    auto type = g_type_map.find(hash);
    if (type != nullptr)
    {
        return type->value;
    }

    return &unknown_type;
}

BEE_CORE_API void reflection_register_builtin_types()
{
#define BUILTIN(builtin_type) get_type<builtin_type>(),

    static const Type* builtin_types[] { BUILTIN_TYPES };

    for (auto& type : builtin_types)
    {
        register_type(type);
    }
}

void register_type(const Type* type)
{
    if (g_type_map.find(type->hash) == nullptr)
    {
        g_type_map.insert(type->hash, type);
    }
}

const char* reflection_flag_to_string(const Qualifier qualifier)
{
#define QUALIFIER(x) case Qualifier::x: return "Qualifier::" #x

    switch (qualifier)
    {
        QUALIFIER(cv_const);
        QUALIFIER(cv_volatile);
        QUALIFIER(lvalue_ref);
        QUALIFIER(rvalue_ref);
        QUALIFIER(pointer);
        default: break;
    }

    return "Qualifier::none";
#undef QUALIFIER
}

const char* reflection_flag_to_string(const StorageClass storage_class)
{
#define STORAGE_CLASS(x) case StorageClass::x: return "StorageClass::" #x

    switch (storage_class)
    {
        STORAGE_CLASS(auto_storage);
        STORAGE_CLASS(register_storage);
        STORAGE_CLASS(static_storage);
        STORAGE_CLASS(extern_storage);
        STORAGE_CLASS(thread_local_storage);
        STORAGE_CLASS(mutable_storage);
        default: break;
    }

    return "StorageClass::none";
#undef STORAGE_CLASS
}

const char* reflection_type_kind_to_string(const TypeKind type_kind)
{
#define TYPE_KIND(x) case TypeKind::x: return "TypeKind::" #x

    switch (type_kind)
    {
        TYPE_KIND(class_decl);
        TYPE_KIND(struct_decl);
        TYPE_KIND(enum_decl);
        TYPE_KIND(union_decl);
        TYPE_KIND(template_decl);
        TYPE_KIND(field);
        TYPE_KIND(function);
        TYPE_KIND(fundamental);
        default: break;
    }

    return "TypeKind::unknown";
#undef TYPE_KIND
}


} // namespace bee