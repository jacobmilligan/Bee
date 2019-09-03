/*
 *  Random.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"

namespace bee {


/// Equal to (2^32) - 1
constexpr i32 random_max = 2147483647;

struct BEE_CORE_API Xorshift {
    u32     state;

    explicit Xorshift(u32 seed);

    u32     next_u32();
    i32     next_i32();
    float   next_float();
};

template <typename PRNGType>
class RandomGenerator {
public:
    RandomGenerator()
        : underlying_generator_(0)
    {}

    explicit RandomGenerator(const u32 seed)
        : underlying_generator_(seed)
    {}

    i32 next_i32()
    {
        return underlying_generator_.next_i32();
    }

    u32 next_u32()
    {
        return underlying_generator_.next_u32();
    }

    float next_float()
    {
        return underlying_generator_.next_float();
    }

    i32 random_range(const i32 min, const i32 max)
    {
        return (underlying_generator_.next_i32() % (max - min + 1)) + min;
    }

    float random_range(const float min, const float max)
    {
        return (underlying_generator_.next_float() % (max - min + 1)) + min;
    }

    u32 random_unsigned_range(const u32 min, const u32 max)
    {
        return (underlying_generator_.next_u32() % (max - min + 1)) + min;
    }
private:
    PRNGType underlying_generator_;
};




} // namespace bee
