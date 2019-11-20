/*
 *  ReflectionV2.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Enum.hpp"
#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/String.hpp"
#include "Bee/Core/Logger.hpp"
#include "Bee/Core/Hash.hpp"


namespace bee {


#ifdef BEE_COMPILE_REFLECTION
    #define BEE_REFLECT(...) __attribute__((annotate("bee-reflect[" #__VA_ARGS__ "]")))
    #define BEE_DEPRECATED(decl, ...) BEE_REFLECT(deprecated, __VA_ARGS__) decl
#else
    #define BEE_REFLECT(...)
    #define BEE_DEPRECATED(decl, ...)
#endif // BEE_COMPILE_REFLECTION

/*
 * Whenever a new type is added here, BEE_SERIALIZER_INTERFACE must also be updated to match this macro
 */
#define BEE_BUILTIN_TYPES                                       \
    BEE_BUILTIN_TYPE(bool, bool)                                \
    BEE_BUILTIN_TYPE(char, char)                                \
    BEE_BUILTIN_TYPE(signed char, signed_char)                  \
    BEE_BUILTIN_TYPE(unsigned char, unsigned_char)              \
    BEE_BUILTIN_TYPE(short, short)                              \
    BEE_BUILTIN_TYPE(unsigned short, unsigned_short)            \
    BEE_BUILTIN_TYPE(int, int)                                  \
    BEE_BUILTIN_TYPE(unsigned int, unsigned_int)                \
    BEE_BUILTIN_TYPE(long, long)                                \
    BEE_BUILTIN_TYPE(unsigned long, unsigned_long)              \
    BEE_BUILTIN_TYPE(long long, long_long)                      \
    BEE_BUILTIN_TYPE(unsigned long long, unsigned_long_long)    \
    BEE_BUILTIN_TYPE(float, float)                              \
    BEE_BUILTIN_TYPE(double, double)                            \
    BEE_BUILTIN_TYPE(void, void)


BEE_FLAGS(Qualifier, u32)
{
    none        = 0u,
    cv_const    = 1u << 0u,
    cv_volatile = 1u << 1u,
    lvalue_ref  = 1u << 2u,
    rvalue_ref  = 1u << 3u,
    pointer     = 1u << 4u
};

BEE_FLAGS(StorageClass, u32)
{
    none                    = 0u,
    auto_storage            = 1u << 0u,
    register_storage        = 1u << 1u,
    static_storage          = 1u << 2u,
    extern_storage          = 1u << 3u,
    thread_local_storage    = 1u << 4u,
    mutable_storage         = 1u << 5u
};

BEE_FLAGS(TypeKind, u32)
{
    unknown                 = 0u,
    class_decl              = 1u << 0u,
    struct_decl             = 1u << 1u,
    enum_decl               = 1u << 2u,
    union_decl              = 1u << 3u,
    template_decl           = 1u << 4u,
    field                   = 1u << 5u,
    function                = 1u << 6u,
    fundamental             = 1u << 7u
};

enum class AttributeKind
{
    boolean,
    integer,
    floating_point,
    string,
    invalid
};


#define BEE_BUILTIN_TYPE(type, name) name##_kind,
enum class FundamentalKind
{
    BEE_BUILTIN_TYPES
    count
};
#undef BEE_BUILTIN_TYPE


struct Type;


class BEE_CORE_API namespace_iterator
{
public:
    using value_type    = StringView;
    using reference     = StringView&;
    using pointer       = StringView*;

    explicit namespace_iterator(const Type* type);

    explicit namespace_iterator(const StringView& fully_qualified_name);

    namespace_iterator(const namespace_iterator& other) = default;

    namespace_iterator& operator=(const namespace_iterator& other) = default;

    const StringView operator*() const;

    const StringView operator->() const;

    namespace_iterator& operator++();

    bool operator==(const namespace_iterator& other) const;

    inline bool operator!=(const namespace_iterator& other) const
    {
        return !(*this == other);
    }
private:
    const char* current_ { nullptr };
    const char* end_ { nullptr };
    i32         size_ { 0 };

    void next_namespace();

    StringView view() const;
};


struct BEE_CORE_API NamespaceRangeAdapter
{
    const Type* type { nullptr };

    namespace_iterator begin() const;

    namespace_iterator end() const;
};


struct NamespaceRangeFromNameAdapter
{
    const StringView& fully_qualified_name { nullptr };

    namespace_iterator begin() const
    {
        return namespace_iterator(fully_qualified_name);
    }

    namespace_iterator end() const
    {
        return namespace_iterator(StringView(fully_qualified_name.data() + fully_qualified_name.size(), 0));
    }
};


inline NamespaceRangeFromNameAdapter get_namespaces_from_name(const StringView& fully_qualified_type_name)
{
    return NamespaceRangeFromNameAdapter { fully_qualified_type_name };
}

inline StringView get_unqualified_name(const StringView& fully_qualified_name)
{
    const auto last_ns = str::last_index_of(fully_qualified_name, "::");
    return last_ns >= 0 ? str::substring(fully_qualified_name, last_ns + 2) : fully_qualified_name;
}


struct Attribute
{
    union Value
    {
        bool        boolean;
        int         integer;
        float       floating_point;
        const char* string;

        explicit Value(const bool b)
            : boolean(b)
        {}

        explicit Value(const int i)
            : integer(i)
        {}

        explicit Value(const float f)
            : floating_point(f)
        {}

        explicit Value(const char* s)
            : string(s)
        {}
    };

    AttributeKind   kind { AttributeKind::invalid };
    u32             hash { 0 };
    const char*     name { nullptr };
    Value           value { false };

    Attribute() = default;

    Attribute(const AttributeKind new_kind, const u32 new_hash, const char* new_name, const Value& new_value)
        : kind(new_kind),
          hash(new_hash),
          name(new_name),
          value(new_value)
    {}
};


struct Field
{
    u32             hash { 0 };
    size_t          offset { 0 };
    Qualifier       qualifier { Qualifier::none };
    StorageClass    storage_class { StorageClass::none };
    const char*     name { nullptr };
    const Type*     type { nullptr };
    Span<Attribute> attributes;
    i32             version_added { 0 };
    i32             version_removed { limits::max<i32>() };

    Field() = default;

    Field(
        const u32 new_hash,
        const size_t new_offset,
        const Qualifier new_qualifier,
        const StorageClass new_storage_class,
        const char* new_name,
        const Type* new_type,
        const Span<Attribute> new_attributes,
        const i32 new_version_added = 0,
        const i32 new_version_removed = limits::max<i32>()
    ) : hash(new_hash),
        offset(new_offset),
        qualifier(new_qualifier),
        storage_class(new_storage_class),
        name(new_name),
        type(new_type),
        attributes(new_attributes),
        version_added(new_version_added),
        version_removed(new_version_removed)
    {}
};


struct Type
{
    u32                     hash { 0 };
    size_t                  size { 0 };
    size_t                  alignment { 0 };
    TypeKind                kind { TypeKind::unknown };
    const char*             name { nullptr };
    i32                     serialized_version { 0 };

    Type() = default;

    Type(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const TypeKind new_kind,
        const char* new_name,
        const i32 new_serialized_version
    ) : hash(new_hash),
        size(new_size),
        alignment(new_alignment),
        kind(new_kind),
        name(new_name),
        serialized_version(new_serialized_version)
    {}

    template <typename T>
    inline const T* as() const
    {
        static_assert(std::is_base_of_v<Type, T>, "`T` must derive from bee::Type");
        return reinterpret_cast<const T*>(this);
    }

    inline NamespaceRangeAdapter namespaces() const
    {
        return NamespaceRangeAdapter { this };
    }

    inline namespace_iterator namespaces_begin() const
    {
        return namespace_iterator(this);
    }

    inline namespace_iterator namespaces_end() const
    {
        return namespace_iterator(StringView(name + str::length(name), 0));
    }

    inline const char* unqualified_name() const
    {
        return name + str::last_index_of(name, ':') + 1;
    }
};


struct FundamentalType final : public Type
{
    FundamentalKind fundamental_kind { FundamentalKind::count };

    FundamentalType() = default;

    FundamentalType(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const TypeKind new_kind,
        const char* new_name,
        const i32 new_serialized_version,
        const FundamentalKind new_fundamental_kind
    ) : Type(new_hash, new_size, new_alignment, new_kind, new_name, new_serialized_version),
        fundamental_kind(new_fundamental_kind)
    {}
};


struct EnumConstant
{
    const char* name { nullptr };
    i64         value { 0 };
    const Type* underlying_type { nullptr };

    EnumConstant() = default;

    EnumConstant(const char* new_name, const i64 new_value, const Type* new_underlying_type)
        : name(new_name),
          value(new_value),
          underlying_type(new_underlying_type)
    {}
};

struct EnumType : public Type
{
    bool                is_scoped { false };
    Span<EnumConstant>  constants;
    Span<Attribute>     attributes;

    EnumType() = default;

    EnumType(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const TypeKind new_kind,
        const char* new_name,
        const i32 new_serialized_version,
        const bool scoped,
        const Span<EnumConstant> new_constants,
        const Span<Attribute> new_attributes
    ) : Type(new_hash, new_size, new_alignment, new_kind, new_name, new_serialized_version),
        is_scoped(scoped),
        constants(new_constants),
        attributes(new_attributes)
    {}
};


struct FunctionType : public Type
{
    StorageClass        storage_class { StorageClass::none };
    bool                is_constexpr { false };
    Field               return_value;
    Span<Field>         parameters;
    Span<Attribute>     attributes;

    using Type::Type;

    FunctionType() = default;

    FunctionType(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const TypeKind new_kind,
        const char* new_name,
        const i32 new_serialized_version,
        const StorageClass new_storage_class,
        const bool make_constexpr,
        const Field& new_return_value,
        const Span<Field> new_parameters,
        const Span<Attribute> new_attributes
    ) : Type(new_hash, new_size, new_alignment, new_kind, new_name, new_serialized_version),
        storage_class(new_storage_class),
        is_constexpr(make_constexpr),
        return_value(new_return_value),
        parameters(new_parameters),
        attributes(new_attributes)
    {}
};


struct RecordType : public Type
{
    Span<Field>                 fields;
    Span<FunctionType>          functions;
    Span<Attribute>             attributes;
    Span<const EnumType*>       enums;
    Span<const RecordType*>     records;
    Span<Type*>                 template_arguments;
    bool                        is_template { false };


    using Type::Type;

    RecordType() = default;

    RecordType(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const TypeKind new_kind,
        const char* new_name,
        const i32 new_serialized_version,
        const Span<Field>& new_fields,
        const Span<FunctionType> new_functions,
        const Span<Attribute> new_attributes,
        const Span<const EnumType*> nested_enums,
        const Span<const RecordType*> nested_records
    ) : Type(new_hash, new_size, new_alignment, new_kind, new_name, new_serialized_version),
        fields(new_fields),
        functions(new_functions),
        attributes(new_attributes),
        enums(nested_enums),
        records(nested_records)
    {}
};

struct UnknownType : public Type
{
    UnknownType()
        : Type(0, 0, 0, TypeKind::unknown, "bee::UnknownType", 0)
    {}
};


extern void reflection_init();

template <typename T>
const Type* get_type();

BEE_CORE_API u32 get_type_hash(const StringView& type_name);

BEE_CORE_API const Type* get_type(const u32 hash);

template <typename ReflectedType, typename T>
inline const T* get_type_as()
{
    return get_type<ReflectedType>()->as<T>();
}

BEE_CORE_API void reflection_register_builtin_types();

BEE_CORE_API void register_type(const Type* type);

BEE_CORE_API const Attribute* find_attribute(const Type* type, const char* attribute_name);

BEE_CORE_API const Attribute* find_attribute(const Field& field, const char* attribute_name);

BEE_CORE_API const Attribute* find_attribute(const Type* type, const char* attribute_name, const AttributeKind kind);

BEE_CORE_API const Attribute* find_attribute(const Field& field, const char* attribute_name, const AttributeKind kind);

BEE_CORE_API const Attribute* find_attribute(const Type* type, const char* attribute_name, const AttributeKind kind, const Attribute::Value& value);

BEE_CORE_API const Attribute* find_attribute(const Field& field, const char* attribute_name, const AttributeKind kind, const Attribute::Value& value);

BEE_CORE_API const char* reflection_flag_to_string(const Qualifier qualifier);

BEE_CORE_API const char* reflection_flag_to_string(const StorageClass storage_class);

BEE_CORE_API const char* reflection_type_kind_to_string(const TypeKind type_kind);

BEE_CORE_API const char* reflection_type_kind_to_code_string(const TypeKind type_kind);

BEE_CORE_API const char* reflection_attribute_kind_to_string(const AttributeKind attr_kind);

template <typename FlagType>
const char* reflection_dump_flags(const FlagType flag)
{
    static thread_local char buffer[4096];
    io::StringStream stream(buffer, static_array_length(buffer), 0);

    int count = 0;
    for_each_flag(flag, [&](const FlagType& f)
    {
        stream.write_fmt(" %s |", reflection_flag_to_string(f));
        ++count;
    });

    if (count == 0)
    {
        stream.write(reflection_flag_to_string(static_cast<FlagType>(0u)));
    }

    if (buffer[stream.size() - 1] == '|')
    {
        buffer[stream.size() - 1] = '\0';
    }

    // Skip the first space that occurs when getting multiple flags
    return count > 0 ? buffer + 1 : buffer;
}


} // namespace bee