/*
 *  OtherHeader.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/ReflectionV2.hpp"

struct BEE_REFLECT() BaseClass
{
    virtual int get_int() = 0;
};