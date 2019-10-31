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


struct Attribute {};


struct Field
{
    size_t          offset { 0 };
    Qualifier       qualifier { Qualifier::none };
    StorageClass    storage_class { StorageClass::none };
    const char*     name { nullptr };
    const Type*     type { nullptr };
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
};


struct EnumConstant
{
    const char* name { nullptr };
    i64         value { 0 };
    const Type* underlying_type { nullptr };
};

struct EnumType : public Type
{
    bool                is_scoped { false };
    Span<EnumConstant>  constants;
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
    Span<Field>          fields;
    Span<FunctionType>   functions;

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

BEE_CORE_API void reflection_initv2();

BEE_CORE_API const char* reflection_flag_to_string(const bee::Qualifier qualifier);

BEE_CORE_API const char* reflection_flag_to_string(const bee::StorageClass storage_class);

BEE_CORE_API const char* reflection_type_kind_to_string(const bee::TypeKind type_kind);

template <typename FlagType>
const char* reflection_dump_flags(const FlagType flag)
{
    static thread_local char buffer[4096];
    bee::io::StringStream stream(buffer, bee::static_array_length(buffer), 0);

    int count = 0;
    bee::for_each_flag(flag, [&](const FlagType& f)
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