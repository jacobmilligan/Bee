//
//  Random.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 6/01/2019
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#include "Bee/Core/Random.hpp"

#include <time.h>

namespace bee {


Xorshift::Xorshift(const u32 seed)
    : state(seed)
{
    if (seed == 0) {
        // unsigned conversion: over/underflow doesn't matter here as this is just a seed value
        state = static_cast<u32>(time(nullptr));
    }
}

float Xorshift::next_float()
{
    // mask off the MSB to allow room for the sign bit
    return sign_cast<float>(next_u32() & random_max);
}

i32 Xorshift::next_i32()
{
    // mask off the MSB to allow room for the sign bit
    return sign_cast<i32>(next_u32() & random_max);
}

u32 Xorshift::next_u32()
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}


} // namespace bee
