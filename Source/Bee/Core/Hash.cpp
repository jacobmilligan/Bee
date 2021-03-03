/*
 *  Hash.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Hash.hpp"

namespace bee {


u32 get_hash(const void* input, const size_t length, const u32 seed)
{
    return XXH32(input, length, seed);
}

u64 get_hash64(const void* input, const size_t length, const u64 seed)
{
    return XXH64(input, length, seed);
}

u128 get_hash128(const void* input, const size_t length, const u64 seed)
{
    const auto hash = XXH128(input, length, seed);
    return u128(hash.low64, hash.high64);
}

HashState::HashState()
    : HashState(0xF00D)
{}

HashState::HashState(const u32 seed)
{
    XXH32_reset(&state_, seed);
}

HashState::HashState(bee::HashState&& other) noexcept
    : state_(other.state_)
{
    XXH32_reset(&other.state_, 0xF00D);
}

HashState& HashState::operator=(bee::HashState&& other) noexcept
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

HashState128::HashState128()
    : HashState128(0xF00D)
{}

HashState128::HashState128(const u64 seed)
{
    XXH3_128bits_reset_withSeed(&state_, seed);
}

HashState128::HashState128(bee::HashState128&& other) noexcept
    : state_(other.state_)
{
    XXH3_128bits_reset(&other.state_);
}

HashState128& HashState128::operator=(bee::HashState128&& other) noexcept
{
    state_ = other.state_;
    XXH3_128bits_reset(&other.state_);
    return *this;
}

void HashState128::add(const void* input, const u32 size)
{
    const auto error = XXH3_128bits_update(&state_, input, size);
    BEE_ASSERT(error != XXH_ERROR);
}

u128 HashState128::end()
{
    const auto hash = XXH3_128bits_digest(&state_);
    return { hash.low64, hash.high64 };
}


} // namespace bee

