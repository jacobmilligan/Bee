/*
 *  ArrayTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Containers/Array.hpp>
#include <Bee/Core/Memory/MallocAllocator.hpp>

#include <gtest/gtest.h>


TEST(ArrayTests, array_constructors_work)
{
    bee::MallocAllocator allocator;

    bee::DynamicArray<int> arr(5, 0, &allocator);
    arr[0] = 23;
    auto arr2 = arr;
    // Check copy assignment
    ASSERT_TRUE(arr2[0] == 23);
    auto arr3 = std::move(arr2);
    // Check move assignment
    ASSERT_TRUE(arr3[0] == 23);
    ASSERT_TRUE(arr2.data() == nullptr);
    ASSERT_TRUE(arr2.capacity() == 0);

    bee::DynamicArray<int> arr4(arr3);
    ASSERT_TRUE(arr4[0] == 23); // check copy constructor
    bee::DynamicArray<int> arr5(std::move(arr4));
    ASSERT_TRUE(arr5[0] == 23); // check move constructor
    ASSERT_TRUE(arr4.data() == nullptr);
    ASSERT_TRUE(arr4.capacity() == 0);
}

TEST(ArrayTests, array_resizes_correctly)
{
    bee::MallocAllocator allocator;
    bee::DynamicArray<int> arr(5, 0, &allocator);
    arr[4] = 23;
    ASSERT_TRUE(arr.capacity() == 5);

    arr.resize(10);
    ASSERT_TRUE(arr[4] == 23);
    ASSERT_TRUE(arr.capacity() == 10);
}

TEST(ArrayTests, array_pushes_and_pops_correctly)
{
    bee::MallocAllocator allocator;
    bee::DynamicArray<int> arr(&allocator);
    for (int i = 0; i < 50; i++) {
        arr.push_back(i);
    }
    ASSERT_TRUE(arr.size() == 50);
    ASSERT_TRUE(arr.capacity() == 64);
    for (int i = 0; i < 50; i++) {
        ASSERT_TRUE(arr[i] == i);
    }

    for (int i = 0; i < 50; i++) {
        arr.pop_back();
    }

    ASSERT_TRUE(arr.size() == 0);
    ASSERT_TRUE(arr.capacity() == 64);
}

TEST(ArrayTests, array_emplaces_correctly)
{
    bee::MallocAllocator allocator;
    bee::DynamicArray<int> arr(&allocator);
    for (int i = 0; i < 50; i++) {
        auto val = i;
        arr.emplace_back(val);
    }
    ASSERT_TRUE(arr.size() == 50);
    ASSERT_TRUE(arr.capacity() == 64);
    for (int i = 0; i < 50; i++) {
        ASSERT_TRUE(arr[i] == i);
    }
}

TEST(ArrayTests, array_range_based_for_works)
{
    bee::MallocAllocator allocator;
    bee::DynamicArray<int> arr(&allocator);
    for (int i = 0; i < 50; i++) {
        arr.push_back(i);
    }

    int count = 0;
    for (auto& i : arr) {
        ASSERT_TRUE(i == count++);
    }
    ASSERT_TRUE(count = 50);
}

struct TestDestruct {
    static int initialized_objects;

    TestDestruct()
    {
        ++initialized_objects;
    }

    ~TestDestruct()
    {
        --initialized_objects;
    }
};
int TestDestruct::initialized_objects = 0;

TEST(ArrayTests, array_resize_smaller_destructs)
{
    static constexpr int max_objects = 100;

    bee::MallocAllocator allocator;
    bee::DynamicArray<TestDestruct> array(&allocator);

    for (int i = 0; i < max_objects; ++i) {
        array.emplace_back();
    }

    ASSERT_EQ(TestDestruct::initialized_objects, max_objects);

    array.resize(max_objects / 2);

    ASSERT_EQ(TestDestruct::initialized_objects, max_objects / 2);
}

TEST(ArrayTests, fixed_array_asserts_on_overflow)
{
    static constexpr int max_objects = 100;

    bee::MallocAllocator allocator;
    bee::FixedArray<int> array(100, &allocator);

    for (int i = 0; i < max_objects; ++i) {
        array.push_back(i);
    }

    ASSERT_DEATH(array.push_back(23), "FixedArray<T>: new_capacity exceeded the fixed capacity of the array");
    ASSERT_DEATH(array.emplace_back(23), "FixedArray<T>: new_capacity exceeded the fixed capacity of the array");
}

TEST(ArrayTests, array_no_raii)
{
    static constexpr int max_objects = 100;

    bee::MallocAllocator allocator;
    bee::FixedArray<int> array(max_objects, &allocator);

    for (int i = 0; i < max_objects; ++i) {
        array.push_back(i);
    }

    ASSERT_EQ(array.size(), max_objects);

    for (int i = 0; i < max_objects; ++i) {
        array.pop_back_no_destruct();
    }

    ASSERT_TRUE(array.empty());

    int val_sum = 0;
    for (auto& val : array) {
        val_sum += val;
    }

    ASSERT_EQ(val_sum, 0);

    static const auto test_push_back = [&]() {
        for (int i = 0; i < max_objects; ++i) {
            array.push_back_no_construct();
            ASSERT_EQ(array.back(), i);
        }
    };

    test_push_back();

    array.resize_no_raii(0);
    ASSERT_TRUE(array.empty());
    ASSERT_GT(array.capacity(), 0);

    test_push_back();
}
