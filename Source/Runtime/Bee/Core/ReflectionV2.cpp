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
#define BEE_BUILTIN_TYPE(builtin_type, function_name)                   \
    template <> BEE_CORE_API  const Type* get_type<builtin_type>()      \
    {                                                                   \
        static FundamentalType instance                                 \
        {                                                               \
            get_type_hash(#builtin_type),                               \
            sizeof_helper<builtin_type>(),                              \
            alignof_helper<builtin_type>(),                             \
            #builtin_type,                                              \
            1,                                                          \
            FundamentalKind::function_name##_kind                       \
        };                                                              \
        return &instance;                                               \
    }

BEE_BUILTIN_TYPES

#undef BEE_BUILTIN_TYPE


template <> BEE_CORE_API const Type* get_type<bee::UnknownType>()
{
    static UnknownType instance{};
    return &instance;
}


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


u32 get_type_hash(const StringView& type_name)
{
    return get_hash(type_name.data(), type_name.size(), 0xb12e92e);
}


const Type* get_type(const u32 hash)
{
    auto type = g_type_map.find(hash);
    if (type != nullptr)
    {
        return type->value;
    }

    return get_type<bee::UnknownType>();
}

BEE_CORE_API void reflection_register_builtin_types()
{
#define BEE_BUILTIN_TYPE(builtin_type, function_name) get_type<builtin_type>(),

    static const Type* builtin_types[] { BEE_BUILTIN_TYPES };

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

/*
 * Comparing sorted integers is extremely cheap so a linear search is the fastest option here for smaller arrays
 * (less than 500 items) of small structs (i.e. Attribute is 24 bytes). It's still useful to keep the
 * attributes sorted by hash though as this still improves the linear search by ~2x based on the benchmarks done on
 * this data. Also, types usually have <= 5 attributes so doing i.e. a binary search would be a waste of CPU time
 */
const Attribute* find_attribute(const Span<Attribute>& attributes, const char* attribute_name, const AttributeKind kind, const Attribute::Value& value)
{
    const auto hash = get_type_hash(attribute_name);

    for (const Attribute& attr: attributes)
    {
        if (attr.hash == hash && attr.kind == kind && memcmp(&attr.value, &value, sizeof(Attribute::Value)) == 0)
        {
            return &attr;
        }
    }

    return nullptr;
}

const Attribute* find_attribute(const Span<Attribute>& attributes, const char* attribute_name, const AttributeKind kind)
{
    const auto hash = get_type_hash(attribute_name);

    for (const Attribute& attr: attributes)
    {
        if (attr.hash == hash && attr.kind == kind)
        {
            return &attr;
        }
    }

    return nullptr;
}

const Attribute* find_attribute(const Span<Attribute>& attributes, const char* attribute_name)
{
    const auto hash = get_type_hash(attribute_name);

    for (const Attribute& attr: attributes)
    {
        if (attr.hash == hash)
        {
            return &attr;
        }
    }

    return nullptr;
}

const Attribute* find_attribute(const Type* type, const char* attribute_name)
{
    switch(type->kind)
    {
        case TypeKind::class_decl:
        case TypeKind::struct_decl:
        case TypeKind::union_decl:
        {
            return find_attribute(reinterpret_cast<const RecordType*>(type)->attributes, attribute_name);
        }
        case TypeKind::enum_decl:
        {
            return find_attribute(reinterpret_cast<const EnumType*>(type)->attributes, attribute_name);
        }
        case TypeKind::function:
        {
            return find_attribute(reinterpret_cast<const FunctionType*>(type)->attributes, attribute_name);
        }
        default: break;
    }

    return nullptr;
}

const Attribute* find_attribute(const Type* type, const char* attribute_name, const AttributeKind kind)
{
    switch(type->kind)
    {
        case TypeKind::class_decl:
        case TypeKind::struct_decl:
        case TypeKind::union_decl:
        {
            return find_attribute(reinterpret_cast<const RecordType*>(type)->attributes, attribute_name, kind);
        }
        case TypeKind::enum_decl:
        {
            return find_attribute(reinterpret_cast<const EnumType*>(type)->attributes, attribute_name, kind);
        }
        case TypeKind::function:
        {
            return find_attribute(reinterpret_cast<const FunctionType*>(type)->attributes, attribute_name, kind);
        }
        default: break;
    }

    return nullptr;
}

const Attribute* find_attribute(const Type* type, const char* attribute_name, const AttributeKind kind, const Attribute::Value& value)
{
    switch(type->kind)
    {
        case TypeKind::class_decl:
        case TypeKind::struct_decl:
        case TypeKind::union_decl:
        {
            return find_attribute(reinterpret_cast<const RecordType*>(type)->attributes, attribute_name, kind, value);
        }
        case TypeKind::enum_decl:
        {
            return find_attribute(reinterpret_cast<const EnumType*>(type)->attributes, attribute_name, kind, value);
        }
        case TypeKind::function:
        {
            return find_attribute(reinterpret_cast<const FunctionType*>(type)->attributes, attribute_name, kind, value);
        }
        default: break;
    }

    return nullptr;
}

const Attribute* find_attribute(const Field& field, const char* attribute_name, const AttributeKind kind)
{
    return find_attribute(field.attributes, attribute_name, kind);
}

const Field* find_field(const Span<Field>& fields, const char* name)
{
    const auto hash = get_type_hash(name);

    for (const Field& field : fields)
    {
        if (field.hash == hash)
        {
            return &field;
        }
    }

    return nullptr;
}


const char* reflection_flag_to_string(const Qualifier qualifier)
{
#define REFL_FLAG(x) case Qualifier::x: return "Qualifier::" #x

    switch (qualifier)
    {
        REFL_FLAG(none);
        REFL_FLAG(cv_const);
        REFL_FLAG(cv_volatile);
        REFL_FLAG(lvalue_ref);
        REFL_FLAG(rvalue_ref);
        REFL_FLAG(pointer);
        default:
        {
            BEE_UNREACHABLE("Missing Qualifier string representation");
        }
    }
#undef REFL_FLAG
}

const char* reflection_flag_to_string(const StorageClass storage_class)
{
#define REFL_FLAG(x) case StorageClass::x: return "StorageClass::" #x

    switch (storage_class)
    {
        REFL_FLAG(none);
        REFL_FLAG(auto_storage);
        REFL_FLAG(register_storage);
        REFL_FLAG(static_storage);
        REFL_FLAG(extern_storage);
        REFL_FLAG(thread_local_storage);
        REFL_FLAG(mutable_storage);
        default:
        {
            BEE_UNREACHABLE("Missing StorageClass string representation");
        }
    }
#undef REFL_FLAG
}

const char* reflection_flag_to_string(const SerializationFlags serialization_flags)
{
#define REFL_FLAG(x) case SerializationFlags::x: return "SerializationFlags::" #x

    switch (serialization_flags)
    {
        REFL_FLAG(none);
        REFL_FLAG(packed_format);
        REFL_FLAG(table_format);
        REFL_FLAG(uses_builder);
        default:
        {
            BEE_UNREACHABLE("Missing SerializationFlags string representation");
        }
    }
#undef REFL_FLAG
}

const char* reflection_flag_to_string(const TypeKind type_kind)
{
#define TYPE_KIND(x) case TypeKind::x: return "TypeKind::" #x

    switch (type_kind)
    {
        TYPE_KIND(unknown);
        TYPE_KIND(class_decl);
        TYPE_KIND(struct_decl);
        TYPE_KIND(enum_decl);
        TYPE_KIND(union_decl);
        TYPE_KIND(template_decl);
        TYPE_KIND(field);
        TYPE_KIND(function);
        TYPE_KIND(fundamental);
        TYPE_KIND(array);
        TYPE_KIND(template_parameter);
        default:
        {
            BEE_UNREACHABLE("Missing TypeKind string representation");
        }
    }
#undef TYPE_KIND
}


const char* reflection_type_kind_to_code_string(const TypeKind type_kind)
{
#define TYPE_KIND(x, str) if ((type_kind & TypeKind::x) != TypeKind::unknown) return str

    TYPE_KIND(class_decl, "class");
    TYPE_KIND(struct_decl, "struct");
    TYPE_KIND(enum_decl, "enum class");
    TYPE_KIND(union_decl, "union");

    return "";
#undef TYPE_KIND
}


BEE_TRANSLATION_TABLE(reflection_attribute_kind_to_string, AttributeKind, const char*, AttributeKind::invalid,
    "AttributeKind::boolean",           // boolean
    "AttributeKind::integer",           // integer
    "AttributeKind::floating_point",    // floating_point
    "AttributeKind::string",            // string
    "AttributeKind::type",              // type
)


} // namespace bee