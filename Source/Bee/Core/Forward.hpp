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


#define BEE_FORWARD(...) static_cast<typename ::bee::detail::remove_ref<decltype(__VA_ARGS__)>::type&&>(__VA_ARGS__)


} // namespace bee