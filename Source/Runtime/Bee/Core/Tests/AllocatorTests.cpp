/*
 *  AllocatorTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Memory/LinearAllocator.hpp>

#include <gtest/gtest.h>
#include <Bee/Core/Memory/VariableSizedPoolAllocator.hpp>
#include <Bee/Core/Memory/PoolAllocator.hpp>
#include <Bee/Core/Containers/Array.hpp>

TEST(AllocatorTests, linear_allocator)
{
    bee::LinearAllocator allocator(128);
    // check capacity minus headers
    for (size_t alloc_idx = 0; alloc_idx < allocator.capacity() / (sizeof(size_t) + 1); ++alloc_idx)
    {
        ASSERT_NO_FATAL_FAILURE(allocator.allocate(1));
    }

    ASSERT_DEATH(allocator.allocate(23), "reached capacity");

    allocator.reset();

    ASSERT_NO_FATAL_FAILURE(allocator.allocate(allocator.max_allocation()));

    allocator.reset();

    auto ptr = allocator.allocate(16);
    ASSERT_TRUE(allocator.is_valid(ptr));

    int value = 23;
    auto invalid_ptr = &value;
    ASSERT_FALSE(allocator.is_valid(invalid_ptr));
    invalid_ptr = nullptr;
    ASSERT_FALSE(allocator.is_valid(invalid_ptr));
}

TEST(AllocatorTests, variable_sized_pool_allocator)
{
    bee::VariableSizedPoolAllocator allocator(1, 512, 256);

    // Test single values
    auto new_int = static_cast<int*>(allocator.allocate(sizeof(int)));
    *new_int = 23;
    ASSERT_EQ(allocator.allocated_size(), sizeof(int));
    allocator.deallocate(new_int);
    ASSERT_EQ(allocator.allocated_size(), 0);

    // Test bucket capacities
    size_t alloc_size = 0;
    void* alloc = nullptr;
    for (size_t j = 0; j < allocator.chunk_count(); ++j)
    {
        const auto bucket_size = 1u << j;
        for (int i = 0; i < allocator.item_count_per_chunk(); ++i)
        {
            alloc = allocator.allocate(bucket_size);
            alloc_size += bucket_size;
            ASSERT_EQ(allocator.allocated_size(), alloc_size) << "Index: " << i << ". Bucket size: " << bucket_size;
        }
        ASSERT_DEATH(allocator.allocate(bucket_size), "Pool memory is exhausted");
    }

    allocator.reset();

    ASSERT_EQ(allocator.allocated_size(), 0);

    for (int i = 0; i < allocator.item_count_per_chunk(); ++i)
    {
        alloc = allocator.allocate(8);
        ASSERT_NE(alloc, nullptr);

        for (int j = 0; j < allocator.item_count_per_chunk() - 1; ++j)
        {
            auto inner_alloc = allocator.allocate(8);
            ASSERT_NE(inner_alloc, nullptr);
            ASSERT_NE(inner_alloc, alloc);
        }

        allocator.reset();
    }

    ASSERT_EQ(allocator.allocated_size(), 0);

    // Test single bucket - fixed size pool
    allocator = bee::VariableSizedPoolAllocator(512, 512, 1024);
    ASSERT_EQ(allocator.capacity(), 557104); // (512 + sizeof(Allocation)) * 1024 + sizeof(size_t) + sizeof(Chunk)
    ASSERT_DEATH(allocator.allocate(256), "Allocation size was smaller");
    ASSERT_DEATH(allocator.allocate(623), "Allocation size exceeds");
    ASSERT_DEATH(allocator.allocate(513), "Allocation size exceeds"); // really close to 512
    ASSERT_NO_FATAL_FAILURE(allocator.allocate(257)); // sits inside the 512 bucket
    ASSERT_NO_FATAL_FAILURE(allocator.allocate(512));
    ASSERT_EQ(allocator.allocated_size(), 512 * 2);
    ASSERT_EQ(allocator.chunk_count(), 1);
}

TEST(AllocatorTests, pool_allocator)
{
    struct TestData
    {
        int     intval { 0 };
        float   floatval { 0 };
        char    str[256];
    };
    // Test allocations happen correctly
    constexpr int num_allocs = 10;

    bee::PoolAllocator pool(bee::get_page_size() * 4, alignof(TestData), num_allocs);
    ASSERT_EQ(pool.allocated_chunk_count(), num_allocs);
    ASSERT_EQ(pool.available_chunk_count(), num_allocs);
    ASSERT_EQ(pool.allocated_chunk_count() - pool.available_chunk_count(), 0);

    TestData* test_data[num_allocs];
    for (auto& data : test_data)
    {
        data = static_cast<TestData*>(pool.allocate(sizeof(TestData)));
        ASSERT_NE(data, nullptr);
    }

    ASSERT_EQ(pool.allocated_chunk_count(), num_allocs);
    ASSERT_EQ(pool.available_chunk_count(), 0);
    ASSERT_EQ(pool.allocated_chunk_count() - pool.available_chunk_count(), num_allocs);

    // Test chunks are correctly recycled
    pool.reset();
    ASSERT_EQ(pool.allocated_chunk_count(), num_allocs);
    ASSERT_EQ(pool.available_chunk_count(), num_allocs);
    ASSERT_EQ(pool.allocated_chunk_count() - pool.available_chunk_count(), 0);

    // allocate blocks and make sure all are different addresses_
    for (auto& data : test_data)
    {
        data = static_cast<TestData*>(pool.allocate(sizeof(TestData)));
        ASSERT_NE(data, nullptr);
    }

    for (int d = 0; d < bee::static_array_length(test_data); ++d)
    {
        for (int other_idx = 0; other_idx < bee::static_array_length(test_data); ++other_idx)
        {
            if (d == other_idx)
            {
                continue;
            }

            ASSERT_NE(test_data[d], test_data[other_idx]);
        }
    }

    ASSERT_EQ(pool.available_chunk_count(), 0);

    int iteration = 1;
    for (auto& data : test_data)
    {
        pool.deallocate(data);
        ASSERT_EQ(pool.available_chunk_count(), iteration) << "i: " << iteration;
        ++iteration;
    }

    ASSERT_EQ(pool.available_chunk_count(), num_allocs);

    // Stress test
    pool = bee::PoolAllocator(4096, alignof(TestData), 32);
    const auto chunk_count = pool.available_chunk_count();
    bee::DynamicArray<void*> chunks;
    for (int i = 0; i < chunk_count * 2; ++i)
    {
        chunks.push_back(pool.allocate(1));
    }

    for (auto& chunk : chunks)
    {
        pool.deallocate(chunk);
    }

    ASSERT_NO_FATAL_FAILURE(pool.~PoolAllocator());
}
