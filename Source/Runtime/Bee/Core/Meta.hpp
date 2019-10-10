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


} // namespace bee