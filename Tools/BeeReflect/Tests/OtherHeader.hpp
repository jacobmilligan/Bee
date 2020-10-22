/*
 *  OtherHeader.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection-Deprecated.hpp"

struct BEE_REFLECT() BaseClass
{
    virtual int get_int() = 0;
};

enum class BEE_REFLECT(float_value = 2.0f) AnotherOne
{
    A,
    B,
    C
};