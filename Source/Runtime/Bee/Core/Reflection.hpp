/*
 *  Reflection.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Hash.hpp"

namespace bee {


/*
 **************************************************************************************************************
 *
 * # static_type<T>
 *
 * Provides static and compile-time information about a given type `T`.
 *
 **************************************************************************************************************
 */
template <typename T>
struct static_type
{
private:
#if BEE_COMPILER_MSVC == 1
    static constexpr int parse_name_offset = sizeof("const char *__cdecl bee::static_type<") - 1;
    static constexpr int parse_name_length_offset = sizeof("int __cdecl bee::static_type<") - 1;
    static constexpr int parse_name_length_right_pad = sizeof(">::parse_name_length(void) noexcept") - 1;
#else
    #error static_type not implemented on the current platform
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

    template <typename T>
    static constexpr size_t sizeof_helper()
    {
        return sizeof(T);
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
const char* static_type<T>::annotated_name = get_annotated_name();

template <typename T>
const char* static_type<T>::fully_qualified_name = get_fully_qualified_name();

template <typename T>
const char* static_type<T>::name = get_name();

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


/*
 **************************************************************************************************************
 *
 * # Type
 *
 * Type structure that can be serialized and stored in containers for runtime type introspection. This is
 * more heavy-weight than `static_type<T>` as types are stored and retrieved in a hash map internally and
 * require registration at runtime
 *
 **************************************************************************************************************
 */
struct Type
{
    u32         hash { limits::max<u32>() };
    size_t      size { 0 };
    size_t      alignment { 0 };
    const char* annotated_name { nullptr };
    const char* fully_qualified_name { nullptr };
    const char* name { nullptr };

    Type() = default;

    template <typename T>
    static Type from_static()
    {
        return Type(
            static_type<T>::hash,
            static_type<T>::size,
            alignof_helper<T>(),
            static_type<T>::annotated_name,
            static_type<T>::fully_qualified_name,
            static_type<T>::name
        );
    }

    inline constexpr bool is_valid() const
    {
        return hash != limits::max<u32>() && annotated_name != nullptr && fully_qualified_name != nullptr && name != nullptr;
    }

private:
    Type(const u32 new_hash, const size_t new_size, const size_t new_alignment, const char* new_annotated_name, const char* new_fully_qualified_name, const char* new_name) noexcept
        : hash(new_hash),
          size(new_size),
          alignment(new_alignment),
          annotated_name(new_annotated_name),
          fully_qualified_name(new_fully_qualified_name),
          name(new_name)
    {}
};

inline bool operator<(const Type& lhs, const Type& rhs)
{
    return lhs.hash < rhs.hash;
}

inline bool operator>(const Type& lhs, const Type& rhs)
{
    return lhs.hash > rhs.hash;
}

inline bool operator<=(const Type& lhs, const Type& rhs)
{
    return lhs.hash <= rhs.hash;
}

inline bool operator>=(const Type& lhs, const Type& rhs)
{
    return lhs.hash >= rhs.hash;
}

inline bool operator==(const Type& lhs, const Type& rhs)
{
    return lhs.hash == rhs.hash;
}

inline bool operator!=(const Type& lhs, const Type& rhs)
{
    return lhs.hash != rhs.hash;
}

template <>
struct Hash<Type> {
    inline u32 operator()(const Type& key) const
    {
        return key.hash;
    }
};


struct BEE_CORE_API TypeRegistrationListNode
{
    TypeRegistrationListNode* next { nullptr };
    Type type;

    explicit TypeRegistrationListNode(const Type& new_type);
};


template <typename T>
class TypeRegistration
{
public:
    TypeRegistration()
        : node_(Type::from_static<T>())
    {}

private:
    TypeRegistrationListNode node_;
};


BEE_CORE_API void reflection_init();

BEE_CORE_API void register_type(const Type& type);

template <typename T>
inline void register_type()
{
    register_type(Type::from_static<T>());
}

BEE_CORE_API void unregister_type(const u32 hash);

BEE_CORE_API Type get_type(const u32 hash) noexcept;

template <typename T>
Type get_type() noexcept
{
    return get_type(static_type<T>::hash);
}


#define BEE_REFLECT(type) static const TypeRegistration<type> BeeTypeRegistration_##type;


} // namespace bee