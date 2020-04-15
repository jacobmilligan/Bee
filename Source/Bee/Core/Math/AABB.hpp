/*
 *  AABB.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Math/float3.hpp"

namespace bee {


struct AABB {
    float3 center;
    float3 extents;
};


}