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
};


struct FunctionType : public Type
{
    StorageClass        storage_class { StorageClass::none };
    bool                is_constexpr { false };
    Field               return_value;
    Span<Field>         parameters;
};


struct RecordType : public Type
{
    Span<Field>                 fields;
    Span<const FunctionType*>   functions;
};


BEE_CORE_API void reflection_initv2();

BEE_CORE_API const Type* get_type(const u32 hash);

template <typename T>
constexpr u32 get_type_hash();

template <typename T>
const Type* get_type();


} // namespace bee