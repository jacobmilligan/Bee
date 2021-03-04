/*
 *  MemoryTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Memory/LinearAllocator.hpp>
#include <Bee/Core/Memory/MallocAllocator.hpp>
#include <Bee/Core/Memory/SmartPointers.hpp>

#include <GTest.hpp>

BEE_PUSH_WARNING
    BEE_DISABLE_PADDING_WARNINGS
    #include <thread>
BEE_POP_WARNING

struct TestObjectBase {
    static bool constructed;
};

template <typename T>
struct TestObject : public TestObjectBase {
    T val;

    explicit TestObject(const T& new_val)
        : val(new_val)
    {
        constructed = true;
    }

    ~TestObject()
    {
        constructed = false;
    }
};

bool TestObjectBase::constructed = false;

TEST(MemoryTests, FixedTempAllocator_make_unique_constructs_and_destructs)
{
    bee::LinearAllocator allocator(bee::kilobytes(1));
    // create scope to test deallocation
    TestObject<int>* other = nullptr;
    {
        auto num = bee::make_unique<TestObject<int>>(allocator, 250);
        other = num.get();
        ASSERT_TRUE(num->val == 250);
        ASSERT_TRUE(TestObjectBase::constructed);
    }
    ASSERT_FALSE(TestObjectBase::constructed);
}

TEST(MemoryTests, MallocAllocator_make_unique_constructs_and_destructs)
{
    bee::MallocAllocator allocator;
    // create scope to test deallocation
    TestObject<int>* other = nullptr;
    {
        auto num = bee::make_unique<TestObject<int>>(allocator, 250);
        other = num.get();
        ASSERT_TRUE(num->val == 250);
        ASSERT_TRUE(TestObjectBase::constructed);
    }
    ASSERT_FALSE(TestObjectBase::constructed);
}
