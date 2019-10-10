/*
 *  HalfTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Math/half.hpp>

#include <gtest/gtest.h>

TEST(test_half, test_conversion)
{
    constexpr int num_iters = 1000;

    bee::half val(10.0f);
    auto fval = static_cast<float>(val);
    ASSERT_FLOAT_EQ(fval, 10.0f);

    for (int i = 0; i < num_iters; ++i) {
        val += 1.0f;
        ASSERT_FLOAT_EQ(val.to_float(), fval + static_cast<float>(i) + 1.0f);
    }

    fval = static_cast<float>(bee::half_max);

    val = fval;
    ASSERT_FLOAT_EQ(val.to_float(), fval);
}