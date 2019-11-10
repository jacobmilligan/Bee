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


/*
 ****************************************
 *
 * namespace_iterator - implementation
 *
 ****************************************
 */
namespace_iterator::namespace_iterator(const Type* type)
    : namespace_iterator(type->name)
{}

namespace_iterator::namespace_iterator(const StringView& fully_qualified_name)
    : current_(fully_qualified_name.c_str()),
      end_(fully_qualified_name.data() + fully_qualified_name.size()),
      size_(str::first_index_of(fully_qualified_name, "::"))
{
    if (size_ < 0)
    {
        // If there's no namespace then this should be equal to end()
        current_ = end_;
        size_ = 0;
    }
}

const StringView namespace_iterator::operator*() const
{
    return StringView(current_, size_);
}

const StringView namespace_iterator::operator->() const
{
    return StringView(current_, size_);
}

namespace_iterator& namespace_iterator::operator++()
{
    next_namespace();
    return *this;
}

bool namespace_iterator::operator==(const bee::namespace_iterator& other) const
{
    return current_ == other.current_;
}

StringView namespace_iterator::view() const
{
    return StringView(current_, static_cast<i32>(end_ - current_));
}

void namespace_iterator::next_namespace()
{
    // Type name strings are guaranteed to be null-terminated
    const auto ns = str::first_index_of(view(), "::");
    if (ns > 0)
    {
        current_ += ns + 2;
    }

    const auto next_ns = str::first_index_of(view(), "::");

    if (next_ns > 0)
    {
        size_ = next_ns;
    }
    else
    {
        // either this is the last namespace and we've reached the unqualified type or this is an empty name
        current_ = end_;
        size_ = 0;
    }
}

namespace_iterator NamespaceRangeAdapter::begin() const
{
    return type->namespaces_begin();
}

namespace_iterator NamespaceRangeAdapter::end() const
{
    return type->namespaces_end();
}

/*
 ****************************************
 *
 * Reflection API - implementation
 *
 ****************************************
 */
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


const char* reflection_type_kind_to_code_string(const TypeKind type_kind)
{
#define TYPE_KIND(x, str) case TypeKind::x: return str

    switch (type_kind)
    {
        TYPE_KIND(class_decl, "class");
        TYPE_KIND(struct_decl, "struct");
        TYPE_KIND(enum_decl, "enum class");
        TYPE_KIND(union_decl, "union");
        default: break;
    }

    return "";
#undef TYPE_KIND
}


BEE_TRANSLATION_TABLE(reflection_attribute_kind_to_string, AttributeKind, const char*, AttributeKind::invalid,
    "AttributeKind::boolean",           // boolean
    "AttributeKind::integer",           // integer
    "AttributeKind::floating_point",    // floating_point
    "AttributeKind::string",            // string
)


} // namespace bee