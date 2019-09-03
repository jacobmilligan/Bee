//
//  AABB.hpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 19/03/2019
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#pragma once

#include "Bee/Core/Math/float3.hpp"

namespace bee {


struct AABB {
    float3 center;
    float3 extents;
};


}