/*
 *  Mouse.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Enum.hpp"


namespace bee {


struct MouseButton
{
    enum Enum
    {
        button_1,
        button_2,
        button_3,
        button_4,
        button_5,
        button_6,
        button_7,
        button_8,
        wheel,
        delta,
        position,
        count,
        left = button_1,
        right = button_2,
        middle = button_3
    };

    static constexpr i32 max = static_cast<i32>(MouseButton::count);


    BEE_ENUM_STRUCT(MouseButton)
};


} // namespace bee
