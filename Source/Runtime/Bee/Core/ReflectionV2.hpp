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
    #define BEE_ATTRIBUTE __attribute__((annotate("bee-attribute[]")))
#else
    #define BEE_REFLECT(...)
    #define BEE_ATTRIBUTE
#endif // BEE_COMPILE_REFLECTION


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

enum class AttributeType
{
    invalid,
    empty,
    int_attr,
    float_attr,
    type_attr,
    string_attr
};


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


struct Attribute {};


struct Field
{
    size_t          offset { 0 };
    Qualifier       qualifier { Qualifier::none };
    StorageClass    storage_class { StorageClass::none };
    const char*     name { nullptr };
    const Type*     type { nullptr };

    Field() = default;

    Field(
        const size_t new_offset,
        const Qualifier new_qualifier,
        const StorageClass new_storage_class,
        const char* new_name,
        const Type* new_type
    ) : offset(new_offset),
        qualifier(new_qualifier),
        storage_class(new_storage_class),
        name(new_name),
        type(new_type)
    {}
};


struct Type
{
    u32                     hash { 0 };
    size_t                  size { 0 };
    size_t                  alignment { 0 };
    TypeKind                kind { TypeKind::unknown };
    const char*             name { nullptr };

    Type() = default;

    Type(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const TypeKind new_kind,
        const char* new_name
    ) : hash(new_hash),
        size(new_size),
        alignment(new_alignment),
        kind(new_kind),
        name(new_name)
    {}

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

    EnumType() = default;

    EnumType(
        const u32 new_hash,
        const size_t new_size,
        const size_t new_alignment,
        const TypeKind new_kind,
        const char* new_name,
        const bool scoped,
        const Span<EnumConstant> new_constants
    ) : Type(new_hash, new_size, new_alignment, new_kind, new_name),
        is_scoped(scoped),
        constants(new_constants)
    {}
};


struct FunctionType : public Type
{
    StorageClass        storage_class { StorageClass::none };
    bool                is_constexpr { false };
    Field               return_value;
    Span<Field>         parameters;

    using Type::Type;

    FunctionType() = default;

    FunctionType(
        const u32 hash,
        const size_t size,
        const size_t alignment,
        const TypeKind kind,
        const char* name,
        const StorageClass new_storage_class,
        const bool make_constexpr,
        const Field& new_return_value,
        const Span<Field> new_parameters
    ) : Type(hash, size, alignment, kind, name),
        storage_class(new_storage_class),
        is_constexpr(make_constexpr),
        return_value(new_return_value),
        parameters(new_parameters)
    {}
};


struct RecordType : public Type
{
    Span<Field>         fields;
    Span<FunctionType>  functions;
    Span<Type*>         template_arguments;
    bool                is_template { false };


    using Type::Type;

    RecordType() = default;

    RecordType(
        const u32 hash,
        const size_t size,
        const size_t alignment,
        const TypeKind kind,
        const char* name,
        const Span<Field>& new_fields,
        const Span<FunctionType> new_functions
    ) : Type(hash, size, alignment, kind, name),
        fields(new_fields),
        functions(new_functions)
    {}
};


template <typename T>
constexpr u32 get_type_hash();

template <typename T>
const Type* get_type();

BEE_CORE_API const Type* get_type(const u32 hash);

extern void reflection_init();

BEE_CORE_API void reflection_register_builtin_types();

BEE_CORE_API void register_type(const Type* type);

BEE_CORE_API const char* reflection_flag_to_string(const Qualifier qualifier);

BEE_CORE_API const char* reflection_flag_to_string(const StorageClass storage_class);

BEE_CORE_API const char* reflection_type_kind_to_string(const TypeKind type_kind);

BEE_CORE_API const char* reflection_type_kind_to_code_string(const TypeKind type_kind);

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