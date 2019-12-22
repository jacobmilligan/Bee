/*
 *  ReflectionTestsV2.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/ReflectionV2.hpp>

#include <gtest/gtest.h>

TEST(ReflectionTestsV2, templated_types)
{
    const auto char_array_type = bee::get_type<bee::DynamicArray<char>>();
    ASSERT_EQ(char_array_type->size, sizeof(bee::DynamicArray<char>));
    ASSERT_EQ(char_array_type->alignment, alignof(bee::DynamicArray<char>));

    using nested_t = bee::DynamicArray<bee::DynamicArray<bee::DynamicArray<char>>>;
    const auto nested_array_type = bee::get_type<nested_t>();
    ASSERT_EQ(nested_array_type->size, sizeof(nested_t));
    ASSERT_EQ(nested_array_type->alignment, alignof(nested_t));
}