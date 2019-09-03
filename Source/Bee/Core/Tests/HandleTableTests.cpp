//
//  HandleTableTests.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 15/06/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#include "Bee/Core/Containers/HandleTable.hpp"

#include <gtest/gtest.h>
#include <Bee/Core/Random.hpp>

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

class HandleTableTest : public ::testing::Test {
protected:
    using resource_pool_t = bee::HandleTable<32, MockResourceHandle, MockResource>;

    resource_pool_t resources_;

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(HandleTableTest, handles_are_correctly_allocated)
{
    MockResourceHandle handle{};
    ASSERT_NO_FATAL_FAILURE(handle = resources_.create({}));
    ASSERT_TRUE(resources_.contains(handle));
}

TEST_F(HandleTableTest, handles_are_correctly_deallocated)
{
    MockResourceHandle handle{};
    ASSERT_NO_FATAL_FAILURE(handle = resources_.create({}));
    ASSERT_TRUE(resources_.contains(handle));
    auto resource = resources_[handle];
    ASSERT_TRUE(resource->intval == MockResource::new_intval);
    ASSERT_TRUE(resource->charval == MockResource::new_charval);
    ASSERT_NO_FATAL_FAILURE(resources_.destroy(handle));
    ASSERT_FALSE(resources_.contains(handle));
    ASSERT_TRUE(resource->intval == MockResource::deallocated_intval);
    ASSERT_TRUE(resource->charval == MockResource::deallocated_charval);
}

TEST_F(HandleTableTest, handles_are_exhausted_when_capacity_is_reached)
{
    MockResourceHandle handle{};
    for (int i = 0; i < resource_pool_t::capacity; ++i)
    {
        handle = resources_.create({});
    }
    ASSERT_DEATH(handle = resources_.create({}), "reached capacity");
}

TEST_F(HandleTableTest, handles_are_reused_correctly)
{
    auto first_handle = resources_.create({});
    resources_.destroy(first_handle);

    for (int i = 0; i < resources_.capacity - 1; i++)
    {
        auto handle = resources_.create({});
        resources_.destroy(handle);
    }

    auto recycled_handle = resources_.create({});
    ASSERT_TRUE(first_handle.index() == recycled_handle.index());
    resources_.destroy(recycled_handle);
}

TEST_F(HandleTableTest, reused_handles_detect_version_correctly)
{
    auto handle1 = resources_.create({});
    resources_.destroy(handle1);
    ASSERT_DEATH(resources_[handle1], "handle references destroyed data");

    // allocate and deallocate handles until the original one is recycled
    for (int i = 0; i < resources_.capacity - 1; i++) {
        auto handle = resources_.create({});
        resources_.destroy(handle);
    }

    auto handle2 = resources_.create({});
    ASSERT_TRUE(handle1.index() == handle2.index());
    ASSERT_FALSE(handle1.version() == handle2.version());
    ASSERT_DEATH(resources_[handle1], "handle version is out of date");
    ASSERT_DEATH(resources_.destroy(handle1), "handle version is out of date");
    ASSERT_NO_FATAL_FAILURE(resources_[handle2]);
    ASSERT_NO_FATAL_FAILURE(resources_.destroy(handle2));
}

TEST_F(HandleTableTest, test_index_is_calculated_correctly)
{
    for (bee::u32 i = 0; i < (1u << MockResourceHandle::index_bits / 2); ++i)
    {
        for (bee::u32 v = 0; v < (1u << MockResourceHandle::version_bits / 2); ++v)
        {
            const auto id = (v << 24) | i;
            ASSERT_EQ(MockResourceHandle{ id }.index(), i);
            ASSERT_EQ(MockResourceHandle{ id }.version(), v);
        }
    }
}

TEST_F(HandleTableTest, test_all_resources_can_allocate_and_get)
{
    MockResource* resource = nullptr;
    for (bee::u32 i = 0; i < resources_.capacity; ++i) {
        auto handle = resources_.create({});
        ASSERT_NO_FATAL_FAILURE(resource = resources_[handle]);
    }
}

TEST_F(HandleTableTest, test_multiple_allocations)
{
    MockResourceHandle handle1;
    MockResourceHandle handle2;
    MockResource* resource1 = nullptr;
    MockResource* resource2 = nullptr;
    ASSERT_NO_FATAL_FAILURE(handle1 = resources_.create_uninitialized(&resource1));
    ASSERT_NO_FATAL_FAILURE(handle2 = resources_.create_uninitialized(&resource2));
    ASSERT_NE(handle1, handle2);

    ASSERT_TRUE(handle1.is_valid());
    ASSERT_TRUE(handle2.is_valid());

    resource1 = resources_[handle1];
    resource2 = resources_[handle2];
    resource2->intval = 100;

    ASSERT_NE(resource1, resource2);

    ASSERT_NO_FATAL_FAILURE(resources_.destroy(handle1));

    resource2 = resources_[handle2];
    ASSERT_EQ(resource2->intval, 100);

    ASSERT_NO_FATAL_FAILURE(resources_.destroy(handle2));

    ASSERT_DEATH(resources_[handle1], "references destroyed data");
}

TEST_F(HandleTableTest, stress_test)
{
    MockResourceHandle handles[resources_.capacity];
    MockResource result{};
    for (int i = 0; i < resources_.capacity; ++i)
    {
        ASSERT_NO_FATAL_FAILURE(handles[i] = resources_.create(result));
    }

    ASSERT_EQ(resources_.size(), resources_.capacity);

    bee::RandomGenerator<bee::Xorshift> rand(239458);
    for (int i = 0; i < resources_.capacity; ++i)
    {
        auto& handle = handles[rand.random_range(0, resources_.capacity - 1)];
        if (resources_.contains(handle))
        {
            ASSERT_NO_FATAL_FAILURE(resources_.destroy(handle));
            handle = MockResourceHandle{};
        }
        else
        {
            ASSERT_NO_FATAL_FAILURE(handle = resources_.create(result));
        }
    }

    for (auto& handle : handles)
    {
        if (resources_.contains(handle))
        {
            ASSERT_NO_FATAL_FAILURE(resources_.destroy(handle));
        }
    }

    ASSERT_EQ(resources_.size(), 0);
}
