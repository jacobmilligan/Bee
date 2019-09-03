/*
 *  TypeTraits.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"
#include "Bee/Core/Error.hpp"
#include "Bee/Core/NumericTypes.hpp"

#include <type_traits>

#if BEE_COMPILER_MSVC == 1
    #include <intrin.h>
#endif // BEE_COMPILER_MSVC == 1


namespace bee {
namespace detail {

/*
 * Required by some compilers to guarantee SFINAE is not ignored
 * see: https://en.cppreference.com/w/cpp/types/void_t
 */
template<typename...>
struct __make_void_t {
    using type = void;
};


} // namespace detail


/**
 * # void_t
 *
 * Maps a set of types to a void type if all of them are well-formed, otherwise it will result in a substitute failure
 */
template <typename... Types>
using void_t = typename detail::__make_void_t<Types...>::type;


/**
 * # sizeof_total
 *
 * Gets the sum sizeof() all the types in a parameter pack
 */
template <typename FirstType, typename... RemainingTypes>
struct sizeof_total;

template <typename T>
struct sizeof_total<T>
{
    static constexpr int value  = sizeof(T);
};

template <typename FirstType, typename... RemainingTypes>
struct sizeof_total
{
    static constexpr int value  = sizeof(FirstType) + sizeof_total<RemainingTypes...>::value;
};

template <typename... Types>
static constexpr int sizeof_total_v = sizeof_total<Types...>::value;

/**
 * # BEE_TRANSLATION_TABLE
 *
 * Defines a free function `func_name` that internally holds a static constexpr table for translating from `enum_type`
 * into `native_type`, i.e. bee::gpu::PixelFormat::bgra8 -> VK_FORMAT_B8G8R8A8_UNORM. Allows for branchless,
 * constant-time enum->enum translations in performance-sensitive code. If the non-native enum type is ever
 * changed, the function will static assert that the values have changed and the translation table defined with this
 * macro needs updating
 */
#define BEE_TRANSLATION_TABLE(func_name, enum_type, native_type, max_enum_value, ...)                                                   \
inline native_type func_name(const enum_type value)                                                                                     \
{                                                                                                                                       \
    static constexpr native_type translation_table[] = { __VA_ARGS__ };                                                                 \
    static constexpr size_t table_size = static_array_length(translation_table);                                                        \
    static_assert(table_size == static_cast<size_t>(max_enum_value),                                                                    \
                "Skyrocket: error: the translation table for "#native_type                                                              \
                " is missing entries. Please update to sync with the "#enum_type" enum.");                                              \
    BEE_ASSERT_F_NO_DEBUG_BREAK(static_cast<size_t>(value) < static_cast<size_t>(max_enum_value),                                       \
                              "Invalid value for `"#enum_type"` to `"#native_type"` translation table given: `"#max_enum_value"`");     \
    return translation_table[static_cast<size_t>(value)];                                                                               \
}

/**
 * # BEE_FLAGS
 *
 * Defines a scoped enum class `E` with the underlying type `T` with bitwise operator
 * overloads: `~`, `|`, `^`, `&`
 */
#define __BEE_ENUM_FLAG_OPERATOR(type, underlying, op)                                                \
    inline constexpr type operator op(const type lhs, const type rhs) noexcept                      \
    {                                                                                               \
        return static_cast<type>(static_cast<underlying>(lhs) op static_cast<underlying>(rhs));     \
    }                                                                                               \
                                                                                                    \
    inline constexpr type operator op##=(type& lhs, const type rhs) noexcept                        \
    {                                                                                               \
        (lhs) = (lhs) op (rhs);                                                                     \
        return (lhs);                                                                               \
    }

#define BEE_FLAGS(E, T) enum class  E : T;                        \
    inline constexpr T underlying_flag_type(E cls) noexcept       \
    {                                                             \
        return static_cast<T>(cls);                               \
    }                                                             \
    inline constexpr E operator~(E cls) noexcept                  \
    {                                                             \
        return static_cast<E>(~underlying_flag_type(cls));        \
    }                                                             \
    __BEE_ENUM_FLAG_OPERATOR(E, T, |)                             \
    __BEE_ENUM_FLAG_OPERATOR(E, T, ^)                             \
    __BEE_ENUM_FLAG_OPERATOR(E, T, &)                             \
    enum class E : T

/**
 * # count_trailing_zeroes
 *
 * Count trailing zeroes in a bitmask using each compilers builtin intrinsics.
 * For MSVC implementations see:
 * https://stackoverflow.com/questions/355967/how-to-use-msvc-intrinsics-to-get-the-equivalent-of-this-gcc-code
 */
BEE_FORCE_INLINE u32 count_trailing_zeroes(const u32 value)
{
#if BEE_COMPILER_GCC == 1 || BEE_COMPILER_CLANG == 1
    return __builtin_ctz(value);
#elif BEE_COMPILER_MSVC == 1
    // see: https://stackoverflow.com/questions/355967/how-to-use-msvc-intrinsics-to-get-the-equivalent-of-this-gcc-code
    unsigned long result = 0;
    if (_BitScanForward(&result, value))
    {
        return result;
    }
    return 32u;
#else
    #error Unsupported platform
#endif // BEE_COMPILER_*
};

/**
 * # count_leading_zeroes
 *
 * Count leading zeroes in a bitmask using each compilers builtin intrinsics.
 * For MSVC implementations see:
 * https://stackoverflow.com/questions/355967/how-to-use-msvc-intrinsics-to-get-the-equivalent-of-this-gcc-code
 */
BEE_FORCE_INLINE u32 count_leading_zeroes(const u32 value)
{
#if BEE_COMPILER_GCC == 1 || BEE_COMPILER_CLANG == 1
    return __builtin_clz(value);
#elif BEE_COMPILER_MSVC == 1
    unsigned long result = 0;
    if (_BitScanReverse(&result, value)) {
        return 31u - result;
    }
    return 32u;
#else
    #error Unsupported platform
#endif // BEE_COMPILER_*
}


/**
 * # are_unique_types
 *
 * Checks if a series of types in a parameter list are unique - that is, they define a set.
 */
template <typename... Types>
struct are_unique_types;

template <>
struct are_unique_types<>
{
    static constexpr bool value = true;
};

template <typename FirstType>
struct are_unique_types<FirstType>
{
    static constexpr bool value = true;
};

template <typename FirstType, typename SecondType>
struct are_unique_types<FirstType, SecondType>
{
    static constexpr bool value = !std::is_same<FirstType, SecondType>::value;
};

template <typename FirstType, typename SecondType, typename... Types>
struct are_unique_types<FirstType, SecondType, Types...> {
    static constexpr bool value = are_unique_types<FirstType, SecondType>::value
                               && are_unique_types<FirstType, Types...>::value
                               && are_unique_types<SecondType, Types...>::value;
};

template <typename... Types>
static constexpr bool are_unique_types_v = are_unique_types<Types...>::value;

/**
 * # are_same_type
 *
 * Checks if a series of types in a parameter list are unique - that is, they define a set.
 */
template <typename TypeToMatch, typename... Types>
struct all_types_match;

template <typename TypeToMatch>
struct all_types_match<TypeToMatch>
{
    static constexpr bool value = true;
};

template <typename TypeToMatch, typename T>
struct all_types_match<TypeToMatch, T>
{
    static constexpr bool value = std::is_same<TypeToMatch, T>::value;
};

template <typename TypeToMatch, typename T, typename... Types>
struct all_types_match<TypeToMatch, T, Types...>
{
    static constexpr bool value = all_types_match<TypeToMatch, T>::value
                               && all_types_match<TypeToMatch, Types...>::value;
};

template <typename TypeToMatch, typename... Types>
static constexpr bool all_types_match_v = all_types_match<TypeToMatch, Types...>::value;


/**
 * # get_type_at_index
 *
 * Gets the type in a parameter pack at the specified index
 */
template <int Index, typename... Types>
struct get_type_at_index;

template <typename T>
struct get_type_at_index<0, T>
{
    using type = T;
};

template <typename FirstType, typename... Types>
struct get_type_at_index<0, FirstType, Types...>
{
    using type = FirstType;
};

template <int Index, typename FirstType, typename... Types>
struct get_type_at_index<Index, FirstType, Types...>
{
    static_assert(Index >= 0, "get_type_at_index: Index must be >= 0");
    using type = typename get_type_at_index<Index - 1, Types...>::type;
};

template <int Index, typename... Types>
using get_type_at_index_t = typename get_type_at_index<Index, Types...>::type;


/**
 * # get_index_of_type
 *
 * Gets the index a particular type is located at in a parameter pack
 */
template <typename T, typename... Types>
struct get_index_of_type;

template <typename T, typename... Types>
struct get_index_of_type<T, T, Types...>
{
    static constexpr i32 value = 0;
};

template <typename T, typename FirstType, typename... Types>
struct get_index_of_type<T, FirstType, Types...>
{
    static constexpr i32 value = 1 + get_index_of_type<T, Types...>::value;
};

template <typename T, typename... Types>
static constexpr i32 get_index_of_type_v = get_index_of_type<T, Types...>::value;

/**
 * # has_type
 */
template <typename T, typename... Types>
struct has_type;

template <typename T>
struct has_type<T>
{
    static constexpr bool value = false;
};

template <typename T, typename OtherType>
struct has_type<T, OtherType>
{
    static constexpr bool value = std::is_same<T, OtherType>::value;
};

template <typename T, typename FirstType, typename... Types>
struct has_type<T, FirstType, Types...>
{
    static constexpr bool value = has_type<T, FirstType>::value || has_type<T, Types...>::value;
};

template <typename T, typename... Types>
static constexpr i32 has_type_v = has_type<T, Types...>::value;


/**
 * # is_primitive
 *
 * Equivalent to std::is_fundamental || std::is_enum
 */
template <typename T>
struct is_primitive
{
    static constexpr bool value = std::is_fundamental<T>::value || std::is_enum<T>::value;
};

template <typename T>
static constexpr bool is_primitive_v = is_primitive<T>::value;


/**
 * # underlying_t
 *
 * Gets the underlying type of a scoped enum class
 */
template <typename EnumType>
constexpr typename std::underlying_type<EnumType>::type underlying_t(EnumType e) noexcept
{
    return static_cast<typename std::underlying_type<EnumType>::type>(e);
}


/**
 * # for_each_flag
 *
 * Iterates all the flags set in an enum defined with BEE_FLAGS and
 * calls a function for each flag
 */
template <typename FlagType, typename FuncType>
constexpr void for_each_flag(const FlagType& flags, const FuncType& callback)
{
    // see: https://lemire.me/blog/2018/02/21/iterating-over-set-bits-quickly/
    auto bitmask = static_cast<u32>(flags);
    while (bitmask != 0)
    {
        const auto cur_bit = count_trailing_zeroes(bitmask);
        callback(static_cast<FlagType>(1u << cur_bit));
BEE_PUSH_WARNING
BEE_DISABLE_WARNING_MSVC(4146) // warning C4146: unary minus operator applied to unsigned type, result still unsigned
        bitmask ^= bitmask & -bitmask;
BEE_POP_WARNING
    }
}


/**
 * # decode_flag
 *
 * Returns `return_val` if the flag `flag` is present in the `flag_set`, otherwise returns 0
 */
template <typename FlagType, typename DecodedType>
constexpr DecodedType decode_flag(const FlagType flag_set, const FlagType flag, const DecodedType return_val)
{
    if ((flag_set & flag) != static_cast<FlagType>(0))
    {
        return return_val;
    }
    return static_cast<DecodedType>(0);
}

/**
 * # get_flag_if_true
 *
 * Returns `flag` if `predicate` is true, otherwise returns 0
 */
template <typename FlagType>
inline constexpr FlagType get_flag_if_true(const bool predicate, const FlagType flag)
{
    return predicate ? flag : static_cast<FlagType>(0);
}


} // namespace bee