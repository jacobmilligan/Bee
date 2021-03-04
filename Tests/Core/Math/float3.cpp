/*
 *  float3.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Math/float3.hpp>

#include <GTest.hpp>

TEST(float3Tests, addition_works_correctly)
{
    bee::float3 a(1.0f, 2.0f, 3.0f);
    auto b = a;
    auto result = a + b;
    for (int i = 0; i < bee::float3::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == (a[i] + a[i]));
    }
}

TEST(float3Tests, subtraction_works_correctly)
{
    bee::float3 a(1.0f, 2.0f, 3.0f);
    auto b = a;
    auto result = a - b;
    for (int i = 0; i < bee::float3::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == (a[i] - a[i]));
    }
}

TEST(float3Tests, multiplication_works_correctly)
{
    bee::float3 a(1.0f, 2.0f, 3.0f);
    auto b = a;
    auto result = a * b;
    for (int i = 0; i < bee::float3::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == (a[i] * a[i]));
    }
}

TEST(float3Tests, division_works_correctly)
{
    bee::float3 a(1.0f, 2.0f, 3.0f);
    auto b = a;
    auto result = a / b;
    for (int i = 0; i < bee::float3::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == (a[i] / a[i]));
    }
}

TEST(float3Tests, compound_assignment_works_correctly)
{
    bee::float3 result(0.0f);
    bee::float3 a(1.0f, 2.0f, 3.0f);
    result += a;
    for (int i = 0; i < bee::float3::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == a[i]);
    }
}

TEST(float3Tests, compound_subtraction_works_correctly)
{
    bee::float3 result(0.0f);
    bee::float3 a(1.0f, 2.0f, 3.0f);
    result -= a;
    for (int i = 0; i < bee::float3::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == -a[i]);
    }
}

TEST(float3Tests, compound_multiplication_works_correctly)
{
    bee::float3 result(2.0f);
    bee::float3 a(1.0f, 2.0f, 3.0f);
    result *= a;
    for (int i = 0; i < bee::float3::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == a[i] * 2.0f);
    }
}

TEST(float3Tests, compound_division_works_correctly)
{
    bee::float3 result(2.0f);
    bee::float3 a(1.0f, 2.0f, 3.0f);
    result /= a;
    for (int i = 0; i < bee::float3::num_components; ++i) {
        ASSERT_TRUE(result.components[i] == 2.0f / a[i]);
    }
}