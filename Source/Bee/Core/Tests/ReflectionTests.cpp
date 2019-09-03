//
//  Reflection.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 11/05/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#include <Bee/Core/Reflection.hpp>

#include <gtest/gtest.h>

namespace test_namespace {


struct AStruct {};

struct NestedType
{
    class InnerType {};
};

class AClass {};

enum NakedEnum {};

enum class ScopedEnum
{
    Type1,
    Type2
};


namespace nested {


struct NestedStruct {};


} // namespace nested
} // namespace test_namespace

namespace another_namespace {

struct AStruct {};

}

template <typename T, int AnnotatedNameSize>
void assert_type_info(const char* name, const char* fully_qualified_name, const char(&annotated_name)[AnnotatedNameSize])
{
    ASSERT_STREQ(bee::type_info<T>::name, name);
    ASSERT_STREQ(bee::type_info<T>::fully_qualified_name, fully_qualified_name);
    ASSERT_STREQ(bee::type_info<T>::annotated_name, annotated_name);
    // Test hash using AnnotatedNameSize - 2 as max index - AnnotatedNameSize is equal to the string length + 1 due to null-termination
    ASSERT_EQ(bee::type_info<T>::hash, bee::detail::fnv1a<AnnotatedNameSize - 2>(annotated_name, bee::static_string_hash_seed_default));
}

template <typename T>
void assert_type()
{
    auto type = bee::Type::from<T>();

    ASSERT_EQ(type.hash, bee::type_info<T>::hash);
    ASSERT_STREQ(type.annotated_name, bee::type_info<T>::annotated_name);
    ASSERT_STREQ(type.fully_qualified_name, bee::type_info<T>::fully_qualified_name);
    ASSERT_STREQ(type.name, bee::type_info<T>::name);

    type = bee::type_info<T>::get_type();

    ASSERT_EQ(type.hash, bee::type_info<T>::hash);
    ASSERT_STREQ(type.annotated_name, bee::type_info<T>::annotated_name);
    ASSERT_STREQ(type.fully_qualified_name, bee::type_info<T>::fully_qualified_name);
    ASSERT_STREQ(type.name, bee::type_info<T>::name);
}

#define ASSERT_FUNDAMENTAL_TYPE_INFO(type)          \
    assert_type_info<type>(#type, #type, #type);    \
    assert_type<type>()



TEST(ReflectionTests, fundamental_types)
{
    ASSERT_FUNDAMENTAL_TYPE_INFO(void);
    ASSERT_FUNDAMENTAL_TYPE_INFO(bool);
    ASSERT_FUNDAMENTAL_TYPE_INFO(int);
    ASSERT_FUNDAMENTAL_TYPE_INFO(short);
    ASSERT_FUNDAMENTAL_TYPE_INFO(float);
    ASSERT_FUNDAMENTAL_TYPE_INFO(double);
}


TEST(ReflectionTests, structs)
{
    // Names
    assert_type_info<test_namespace::AStruct>("AStruct", "test_namespace::AStruct", "struct test_namespace::AStruct");
    assert_type_info<another_namespace::AStruct>("AStruct", "another_namespace::AStruct", "struct another_namespace::AStruct");
}

TEST(ReflectionTests, classes)
{
    // Names
    assert_type_info<test_namespace::AClass>("AClass", "test_namespace::AClass", "class test_namespace::AClass");
    assert_type_info<test_namespace::NestedType::InnerType>("InnerType", "test_namespace::NestedType::InnerType", "class test_namespace::NestedType::InnerType");
}

TEST(ReflectionTests, enums)
{
    // Names
    assert_type_info<test_namespace::NakedEnum>("NakedEnum", "test_namespace::NakedEnum", "enum test_namespace::NakedEnum");
    assert_type_info<test_namespace::ScopedEnum>("ScopedEnum", "test_namespace::ScopedEnum", "enum test_namespace::ScopedEnum");
}

