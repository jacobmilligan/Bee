/*
 *  Noncopyable.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"

namespace bee {


struct BEE_API Noncopyable {
    constexpr Noncopyable() = default;
    ~Noncopyable() = default;
    Noncopyable(const Noncopyable& other) = delete;
    Noncopyable& operator=(const Noncopyable& other) = delete;
};


} // namespace bee