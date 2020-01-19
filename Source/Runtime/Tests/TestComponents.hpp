/*
 *  TestComponents.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection.hpp"

namespace bee {


struct BEE_REFLECT() Position
{
    float x { 0.0f };
    float y { 0.0f };
    float z { 0.0f };
};


} // namespace bee