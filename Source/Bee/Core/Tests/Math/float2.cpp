/*
 *  float2.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Math/float2.hpp>

#include <gtest/gtest.h>


TEST(float2Tests, addition_works_correctly)
{
    bee::float2 a(1.0f, 2.0f);
    auto b = a;
    auto result = a + b;
    for (int i = 0; i < bee::float2::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == (a[i] + a[i]));
    }
}

TEST(float2Tests, subtraction_works_correctly)
{
    bee::float2 a(1.0f, 2.0f);
    auto b = a;
    auto result = a - b;
    for (int i = 0; i < bee::float2::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == (a[i] - a[i]));
    }
}

TEST(float2Tests, multiplication_works_correctly)
{
    bee::float2 a(1.0f, 2.0f);
    auto b = a;
    auto result = a * b;
    for (int i = 0; i < bee::float2::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == (a[i] * a[i]));
    }
}

TEST(float2Tests, division_works_correctly)
{
    bee::float2 a(1.0f, 2.0f);
    auto b = a;
    auto result = a / b;
    for (int i = 0; i < bee::float2::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == (a[i] / a[i]));
    }
}

TEST(float2Tests, compound_assignment_works_correctly)
{
    bee::float2 result(0.0f);
    bee::float2 a(1.0f, 2.0f);
    result += a;
    for (int i = 0; i < bee::float2::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == a[i]);
    }
}

TEST(float2Tests, compound_subtraction_works_correctly)
{
    bee::float2 result(0.0f);
    bee::float2 a(1.0f, 2.0f);
    result -= a;
    for (int i = 0; i < bee::float2::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == -a[i]);
    }
}

TEST(float2Tests, compound_multiplication_works_correctly)
{
    bee::float2 result(2.0f);
    bee::float2 a(1.0f, 2.0f);
    result *= a;
    for (int i = 0; i < bee::float2::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == a[i] * 2.0f);
    }
}

TEST(float2Tests, compound_division_works_correctly)
{
    bee::float2 result(2.0f);
    bee::float2 a(1.0f, 2.0f);
    result /= a;
    for (int i = 0; i < bee::float2::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == 2.0f / a[i]);
    }
}