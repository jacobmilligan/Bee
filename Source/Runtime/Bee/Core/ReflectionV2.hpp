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


struct FunctionType final : public Type
{
    StorageClass        storage_class { StorageClass::none };
    bool                is_constexpr { false };
    Field               return_value;
    DynamicArray<Field> parameters;
};


struct RecordType final : public Type
{
    DynamicArray<Field>                 fields;
    DynamicArray<const FunctionType*>   functions;
};

/*
 **************************************************************************************************************
 *
 * # static_type_info<T>
 *
 * Provides static and compile-time information about a given type `T`.
 *
 **************************************************************************************************************
 */
template <typename T>
struct static_type_info
{
private:
#if BEE_COMPILER_MSVC == 1
    static constexpr int parse_name_offset = sizeof("const char *__cdecl bee::static_type_info<") - 1;
    static constexpr int parse_name_length_offset = sizeof("int __cdecl bee::static_type_info<") - 1;
    static constexpr int parse_name_length_right_pad = sizeof(">::parse_name_length(void) noexcept") - 1;
#else
    static constexpr int parse_name_offset = sizeof("static const char *bee::static_type_info<") - 1;
    static constexpr int parse_name_length_offset = sizeof("static int bee::type_info<") - 1;
    static constexpr int parse_name_length_right_pad = sizeof(">::parse_name_length() [T = int]") - 1;
#endif // BEE_COMPILER_* == 1

    static constexpr const char* parse_name() noexcept
    {
        return BEE_FUNCTION_NAME + parse_name_offset;
    }

    static constexpr int parse_name_length() noexcept
    {
        return static_cast<int>(sizeof(BEE_FUNCTION_NAME)) - parse_name_length_offset - parse_name_length_right_pad - 1;
    }

    static const char* get_annotated_name() noexcept
    {
        static constexpr auto length_null_terminated = parse_name_length() + 1;
        static char name_buffer[length_null_terminated] { 0 };

        if (name_buffer[0] != '\0')
        {
            return name_buffer;
        }

        name_buffer[length_null_terminated - 1] = '\0';
        memcpy(name_buffer, parse_name(), length_null_terminated - 1);

        return name_buffer;
    }

    static const char* get_fully_qualified_name() noexcept
    {
        static bool already_parsed = false;
        static const char* name_str = get_annotated_name();

        if (!already_parsed)
        {
            already_parsed = true;
            const char* iter = name_str;
            while (*iter != '\0')
            {
                // Ignore space as start of name if the next character is the start of an array length or pointer qualifier
                if (*iter == ' ' && *(iter + 1) != '[' && *(iter + 1) != '(')
                {
                    name_str = iter + 1;
                }
                ++iter;
            }
        }

        return name_str;
    }

    static const char* get_name() noexcept
    {
        static bool already_parsed = false;
        static const char* name_str = get_fully_qualified_name();

        if (!already_parsed)
        {
            already_parsed = true;
            const char* iter = name_str;
            while (*iter != '\0')
            {
                if (*iter == ':' && *(iter + 1) == ':')
                {
                    name_str = iter + 2;
                }
                ++iter;
            }
        }

        return name_str;
    }

    template <typename U>
    static constexpr size_t sizeof_helper()
    {
        return sizeof(U);
    }

    template <>
    static constexpr size_t sizeof_helper<void>()
    {
        return 0;
    }

public:
    /**
     * A hash of the types fully-qualified name that uniquely identifies this type. Evaluated at compile-time
     */
    static constexpr u32 hash = detail::fnv1a<parse_name_length() - 1>(parse_name(), static_string_hash_seed_default);

    /**
     * The sizeof value of the type `T`
     */
    static constexpr size_t size = sizeof_helper<T>();

    /**
     * The length of the types name string. Evaluated at compile-time
     */
    static constexpr i32 name_length = parse_name_length();

    /**
     * A null-terminated static string containing the types fully-qualified name with any struct/class/enum annotation.
     * For instance, the type:
     * ```cpp
     * namespace examples { struct AType {} };
     * ```
     * would have an `annotated_name` of `"struct examples::AType"`.
     *
     * Evaluated statically at runtime once when first accessed.
     */
    static const char* annotated_name;

    /**
     * A null-terminated static string containing the types fully-qualified name.
     * For instance, the type:
     * ```cpp
     * namespace examples { enum class AnEnum {} };
     * ```
     * would have a `fully_qualified_name` of `"examples::AnEnum"`.
     *
     * Evaluated statically at runtime once when first accessed.
     */
    static const char* fully_qualified_name;

    /**
     * A null-terminated static string containing the types non-qualified name.
     * For instance, the type:
     * ```cpp
     * namespace examples { class AClass { struct InnerStruct {}; } };
     * ```
     * `AClass` would have a `name` of `"AClass"` & `ACLass::InnerStruct` would have a `name` of `InnerStruct`.
     *
     * Evaluated statically at runtime once when first accessed.
     */
    static const char* name;
};


template <typename T>
const char* static_type_info<T>::annotated_name = get_annotated_name();

template <typename T>
const char* static_type_info<T>::fully_qualified_name = get_fully_qualified_name();

template <typename T>
const char* static_type_info<T>::name = get_name();


template <typename T>
inline size_t alignof_helper()
{
    return alignof(T);
}

template <>
inline size_t alignof_helper<void>()
{
    return 0;
}

template <typename T>
inline size_t sizeof_helper()
{
    return sizeof(T);
}

template <>
inline size_t sizeof_helper<void>()
{
    return 0;
}


template <typename T>
constexpr u32 get_type_hash()
{
    return static_type_info<T>::hash;
}

template <typename T>
const char* get_type_name()
{
    return static_type_info<T>::fully_qualified_name;
}

BEE_CORE_API void reflection_register_type(const Type& type);


} // namespace bee