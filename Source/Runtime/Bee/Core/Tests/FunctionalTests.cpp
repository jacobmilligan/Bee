/*
 *  Function.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Functional.hpp>

#include <gtest/gtest.h>

constexpr void test_function_constexpr(int& x)
{
    x *= 2;
}

constexpr int test_function_r_constexpr(int x)
{
    return x * 2;
}

void test_function(int& x)
{
    x *= 2;
}

int test_function_r(int x)
{
    return x * 2;
}

void test_function_noexcept(int& x) noexcept
{
    x *= 2;
}

int test_function_r_noexcept(int x) noexcept
{
    return x * 2;
}

struct TestStruct {
    void test_function(int& x)
    {
        x *= 2;
    }

    int test_function_r(int x)
    {
        return x * 2;
    }

    constexpr void test_function_constexpr(int& x)
    {
        x *= 2;
    }

    constexpr int test_function_r_constexpr(int x)
    {
        return x * 2;
    }

    void test_function_noexcept(int& x) noexcept
    {
        x *= 2;
    }

    int test_function_r_noexcept(int x) noexcept
    {
        return x * 2;
    }
};

TEST(FunctionalTests, is_invocable)
{
    auto invocable = bee::is_invocable<decltype(test_function), int&>::value;
    ASSERT_TRUE(invocable);

    invocable = bee::is_invocable<decltype(test_function_constexpr), int&>::value;
    ASSERT_TRUE(invocable);

    invocable = bee::is_invocable<decltype(test_function_noexcept), int&>::value;
    ASSERT_TRUE(invocable);

    auto invocable_r = bee::is_invocable_r<int, decltype(test_function_r), int>::value;
    ASSERT_TRUE(invocable_r);

    invocable_r = bee::is_invocable_r<int, decltype(test_function_r_constexpr), int>::value;
    ASSERT_TRUE(invocable_r);

    invocable_r = bee::is_invocable_r<int, decltype(test_function_r_noexcept), int>::value;
    ASSERT_TRUE(invocable_r);
}

TEST(FunctionalTests, invoke)
{
    TestStruct struct_instance{};
    auto& reference = struct_instance;
    auto pointer = &struct_instance;

    auto test_struct = [&](int& result, auto function) {
        bee::invoke(function, TestStruct{}, result);
        bee::invoke(function, struct_instance, result);
        bee::invoke(function, reference, result);
        bee::invoke(function, pointer, result);
    };

    auto test_struct_r = [&](auto function) -> int {
        int result = 0;
        result += bee::invoke(function, TestStruct{}, 1);
        result += bee::invoke(function, struct_instance, 1);
        result += bee::invoke(function, reference, 1);
        result += bee::invoke(function, pointer, 1);
        return result;
    };

    int result = 1;
    test_struct(result, &TestStruct::test_function);
    test_struct(result, &TestStruct::test_function_constexpr);
    test_struct(result, &TestStruct::test_function_noexcept);
    ASSERT_EQ(result, 1 << (4 * 3)); // 2^(4 * 3) - 3 tests, 4 subtests

    ASSERT_EQ(test_struct_r(&TestStruct::test_function_r), 8);
    ASSERT_EQ(test_struct_r(&TestStruct::test_function_r_constexpr), 8);
    ASSERT_EQ(test_struct_r(&TestStruct::test_function_r_noexcept), 8);

    result = 1;
    bee::invoke(test_function, result);
    bee::invoke(test_function_noexcept, result);
    bee::invoke(test_function_constexpr, result);
    ASSERT_EQ(result, 1 << 3); // 2^3

    result = 1;
    result += bee::invoke(test_function_r, 2);
    result += bee::invoke(test_function_r_noexcept, 3);
    result += bee::invoke(test_function_r_constexpr, 4);
    ASSERT_EQ(result, 1 + (2 * 2) + (2 * 3) + (2 * 4));

    auto lambda_result = bee::invoke([](int x) -> int { return x * 2; }, 4);
    ASSERT_EQ(lambda_result, 8);

    constexpr int test_closure_base_val = 23;
    int test_closure = 0;
    bee::invoke([&](int x) { test_closure = test_closure_base_val + x; }, 25);
    ASSERT_EQ(test_closure, test_closure_base_val + 25);

    auto lambda = [&](int x) { test_closure = test_closure_base_val + x; };
    bee::invoke(lambda, 12);
    ASSERT_EQ(test_closure, test_closure_base_val + 12);
}

// Test closure
struct TestClosureCaller {
    using closure_function_t = bee::Function<void()>;

    int value { 0 };

    void closure_call(closure_function_t function)
    {
        ++value;
        function();
    }
};

struct TestClosureTarget {
    static TestClosureCaller caller;

    int x { 0 };

    void test_thing()
    {
        caller.closure_call([&]() {
            x += 10;
        });
    }
};

TestClosureCaller TestClosureTarget::caller;

TEST(FunctionalTests, function)
{
    using function_t = bee::Function<void(int&)>;
    using function_r_t = bee::Function<int(int)>;

    // Test constructors
    function_t free_function(test_function);
    int result = 23;
    free_function(result);

    ASSERT_EQ(result, 23 * 2);

    function_r_t function_r([&](int x) { return x * 2; });
    ASSERT_EQ(function_r(23), 23 * 2);

    // Test implicit conversion =
    result = 23;
    function_t lambda_function = [](int& x) { x *= 2; };
    lambda_function(result);
    ASSERT_EQ(result, 23 * 2);

    result = 23;
    function_r_t lambda_function_r = [](int x) -> int { return x * 2; };
    ASSERT_EQ(lambda_function_r(123), 123 * 2);

    TestClosureTarget closure_tester{};
    closure_tester.x = 0;
    for (int iter = 0; iter < 100; ++iter) {
        closure_tester.test_thing();
    }

    ASSERT_EQ(closure_tester.x, 10 * 100);
    ASSERT_EQ(closure_tester.caller.value, 100);

    ASSERT_EQ(sizeof(bee::Function<void()>), 32);
    ASSERT_EQ(sizeof(bee::Function<void(bee::u8[32])>), 32);

BEE_PUSH_WARNING
BEE_DISABLE_WARNING_CLANG("-Wuninitialized")
BEE_DISABLE_WARNING_CLANG("-Wunused-lambda-capture")
    int val1, val2, val3, val4;
    auto lambda_16bytes = [val1, val2, val3, val4](bee::u8[32]) { };
BEE_POP_WARNING

    ASSERT_EQ(sizeof(lambda_16bytes), 16);

    bee::Function<void(bee::u8[32])> func = lambda_16bytes;
    ASSERT_EQ(sizeof(func), 32);

    // Test alignment
    ASSERT_EQ(alignof(bee::Function<void(int), 1>), 8);
    ASSERT_EQ(alignof(bee::Function<void(int), 32>), alignof(std::aligned_storage_t<32>));
    ASSERT_EQ(alignof(bee::Function<void(int), 32, 16>), 16);
    ASSERT_EQ(alignof(bee::Function<void(int), 32, 32>), 32);
}