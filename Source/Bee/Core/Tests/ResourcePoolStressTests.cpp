//
//  GenericResourcePool.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 13/10/18
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#include <Bee/Core/Containers/ResourcePool.hpp>

#include <gtest/gtest.h>

struct MockResource {
    static constexpr int new_intval = -1;
    static constexpr char new_charval = '\0';
    static constexpr int deallocated_intval = -99;
    static constexpr char deallocated_charval = 'x';

    int intval { new_intval };
    char charval { new_charval };

    ~MockResource()
    {
        intval = deallocated_intval;
        charval = deallocated_charval;
    }
};

BEE_DEFINE_VERSIONED_HANDLE(MockResource);

constexpr int MockResource::new_intval;
constexpr char MockResource::new_charval;
constexpr int MockResource::deallocated_intval;
constexpr char MockResource::deallocated_charval;

TEST(ResourcePoolStressTests, stress_test)
{
    bee::ResourcePool<1 << 23, MockResourceHandle, MockResource> stress_test_pool;
    bee::FixedArray<MockResourceHandle> allocated_handles(stress_test_pool.capacity);

    for (bee::u32 i = 0; i < stress_test_pool.capacity; ++i) {
        ASSERT_NO_FATAL_FAILURE(allocated_handles.push_back(stress_test_pool.allocate())) << "Index: " << i;
        ASSERT_EQ(allocated_handles[i].version(), 1u);
    }

    ASSERT_EQ(stress_test_pool.allocated_count(), 1u << 23u);

    for (bee::u32 i = 0; i < stress_test_pool.capacity; ++i) {
        ASSERT_NO_FATAL_FAILURE(stress_test_pool.deallocate(allocated_handles[i])) << "Index: " << i;
    }

    for (bee::u32 i = 0; i < stress_test_pool.capacity; ++i) {
        MockResourceHandle handle{};
        ASSERT_NO_FATAL_FAILURE(handle = stress_test_pool.allocate()) << "Index: " << i;
        ASSERT_EQ(handle.version(), 2u) << "Index: " << i;
        ASSERT_EQ(handle.index(), allocated_handles[i].index()) << "Index: " << i;
    }
}


