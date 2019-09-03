//
//  Hash.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 5/02/2019
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#include "Bee/Core/Hash.hpp"

namespace bee {


u32 get_hash(const void* input, const size_t length, const u32 seed)
{
    return XXH32(input, length, seed);
}

HashState::HashState()
    : HashState(0xF00D)
{}

HashState::HashState(const u32 seed)
{
    XXH32_reset(&state_, seed);
}

HashState::HashState(bee::HashState&& other)
    : state_(other.state_)
{
    XXH32_reset(&other.state_, 0xF00D);
}

HashState& HashState::operator=(bee::HashState&& other)
{
    state_ = other.state_;
    XXH32_reset(&other.state_, 0);
    return *this;
}

void HashState::add(const void* input, const u32 size)
{
    const auto error = XXH32_update(&state_, input, size);
    BEE_ASSERT(error != XXH_ERROR);
}

u32 HashState::end()
{
    return XXH32_digest(&state_);
}


} // namespace bee

