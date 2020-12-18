/*
 *  Forward.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once


namespace bee {
namespace detail {


template<class T>
struct remove_ref { using type = T; };

template<class T>
struct remove_ref<T&> { using type = T; };

template<class T>
struct remove_ref<T&&> { using type = T; };


} // namespace detail

/*
 * # BEE_MOVE
 *
 * this is a replacement for BEE_MOVE used to avoid having to include <utility> (which also includes <type_traits>)
 */
#define BEE_MOVE(...) static_cast<typename ::bee::detail::remove_ref<decltype(__VA_ARGS__)>::type&&>(__VA_ARGS__)


} // namespace bee