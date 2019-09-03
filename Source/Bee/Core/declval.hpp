//
//  declval.hpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 17/08/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#pragma once

namespace bee {
namespace detail {


template <typename T>
struct __add_rvalue_reference
{
    using type = T&&;
};

template <typename T>
struct __add_rvalue_reference<T&>
{
    using type = T&;
};

template <typename T>
struct __add_rvalue_reference<T&&>
{
    using type = T&&;
};

template <>
struct __add_rvalue_reference<void>
{
    using type = void;
};

template <>
struct __add_rvalue_reference<const void>
{
    using type = const void;
};


} // namespace detail


/*
 * # `declval`
 *
 * Minimal implementation of `std::declval` to avoid having to include <utility> (which also includes <type_traits>)
 * to use this functionality
 */
template <typename T>
typename detail::__add_rvalue_reference<T>::type declval() noexcept;


} // namespace bee