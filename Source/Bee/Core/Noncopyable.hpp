//
//  Noncopyable.hpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 11/09/2018
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

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