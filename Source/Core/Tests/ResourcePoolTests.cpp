/*
 *  GenericResourcePool.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

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

BEE_VERSIONED_HANDLE_32(MockResourceHandle);

constexpr int MockResource::new_intval;
constexpr char MockResource::new_charval;
constexpr int MockResource::deallocated_intval;
constexpr char MockResource::deallocated_charval;

class ResourcePoolTests : public ::testing::Test {
protected:
    using resource_pool_t = bee::ResourcePool<MockResourceHandle, MockResource>;
    using id_t = bee::ResourcePool<MockResourceHandle, MockResource>::id_t;
    using handle_t = bee::ResourcePool<MockResourceHandle, MockResource>::handle_t;

    resource_pool_t resources_ { 32 };

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(ResourcePoolTests, handles_are_correctly_allocated)
{
    MockResourceHandle handle{};
    ASSERT_NO_FATAL_FAILURE(handle = resources_.allocate());
    ASSERT_TRUE(resources_.is_active(handle));
}

TEST_F(ResourcePoolTests, handles_are_correctly_deallocated)
{
    MockResourceHandle handle{};
    MockResource* resource = nullptr;
    ASSERT_NO_FATAL_FAILURE(handle = resources_.allocate());
    ASSERT_NO_FATAL_FAILURE(resource = &resources_[handle]);

    ASSERT_TRUE(resource->intval == MockResource::new_intval);
    ASSERT_TRUE(resource->charval == MockResource::new_charval);

    ASSERT_NO_FATAL_FAILURE(resources_.deallocate(handle));
    ASSERT_FALSE(resources_.is_active(handle));
    ASSERT_TRUE(resource->intval == MockResource::deallocated_intval);
    ASSERT_TRUE(resource->charval == MockResource::deallocated_charval);

    ASSERT_NO_FATAL_FAILURE(handle = resources_.allocate());
    ASSERT_NO_FATAL_FAILURE(resource = &resources_[handle]);
    ASSERT_TRUE(resource->intval == MockResource::new_intval);
    ASSERT_TRUE(resource->charval == MockResource::new_charval);

    resources_.clear();

    ASSERT_TRUE(resource->intval == MockResource::deallocated_intval);
    ASSERT_TRUE(resource->charval == MockResource::deallocated_charval);
}

TEST_F(ResourcePoolTests, handles_are_not_exhausted_when_capacity_is_reached)
{
    MockResourceHandle handle{};
    for (id_t i = 0; i < 32; ++i) {
        handle = resources_.allocate();
    }
    ASSERT_EQ(resources_.size(), 32u);
    ASSERT_EQ(resources_.chunk_count(), 8u);
    const auto chunk_capacity = 32 / sizeof(MockResource);
    ASSERT_EQ(resources_.allocated_size(), sizeof(MockResource) * chunk_capacity * 8);
}

TEST_F(ResourcePoolTests, handles_are_reused_correctly)
{
    auto first_handle = resources_.allocate();
    resources_.deallocate(first_handle);

    for (id_t i = 0; i < 33 - 1; i++) {
        auto handle = resources_.allocate();
        resources_.deallocate(handle);
    }

    auto recycled_handle = resources_.allocate();
    ASSERT_TRUE(first_handle.index() == recycled_handle.index());
    resources_.deallocate(recycled_handle);
}

TEST_F(ResourcePoolTests, reused_handles_detect_version_correctly)
{
    auto handle1 = resources_.allocate();
    resources_.deallocate(handle1);
    ASSERT_DEATH(resources_[handle1], "Handle had an invalid index");

    // allocate and deallocate handles until the original one is recycled
    for (id_t i = 0; i < handle_t::generator_t::version_mask - 1; i++) {
        auto handle = resources_.allocate();
        resources_.deallocate(handle);

        ASSERT_EQ(handle.index(), handle1.index());
        ASSERT_NE(handle.version(), handle1.version());
    }

    auto handle2 = resources_.allocate();
    ASSERT_EQ(handle1.index(), handle2.index());
    ASSERT_NE(handle1.version(), handle2.version());
    ASSERT_DEATH(resources_[handle1], "Handle was out of date with the version stored in the resource pool");
    ASSERT_DEATH(resources_.deallocate(handle1), "Attempted to free a resource using an outdated handle");
    ASSERT_NO_FATAL_FAILURE(resources_[handle2]);
    ASSERT_NO_FATAL_FAILURE(resources_.deallocate(handle2));
}

TEST_F(ResourcePoolTests, test_index_is_calculated_correctly)
{
    for (id_t i = 0; i < (1u << MockResourceHandle::generator_t::index_bits / 2); ++i) {
        for (id_t v = 0; v < (1u << MockResourceHandle::generator_t::version_bits / 2); ++v) {
            const auto id = (v << 24u) | i;
            ASSERT_EQ(MockResourceHandle{ id }.index(), i);
            ASSERT_EQ(MockResourceHandle{ id }.version(), v);
        }
    }
}

TEST_F(ResourcePoolTests, test_all_resources_can_allocate_and_get)
{
    for (id_t i = 0; i < resources_.size(); ++i) {
        auto handle = resources_.allocate();
        auto resource = &resources_[handle];
        ASSERT_NE(resource, nullptr);
        resource = nullptr;
        ASSERT_NO_FATAL_FAILURE(resource = &resources_[handle]);
        ASSERT_NE(resource, nullptr);
    }
}

TEST_F(ResourcePoolTests, test_iterator)
{
    for (id_t i = 0; i < 32; ++i)
    {
        const auto handle = resources_.allocate();
        ASSERT_TRUE(handle.is_valid());
    }

    int count = 0;
    for (auto& resource : resources_)
    {
        ASSERT_EQ(resource.intval, MockResource::new_intval);
        ASSERT_EQ(resource.charval, MockResource::new_charval);
        ++count;
    }

    ASSERT_EQ(count, 32);
}

