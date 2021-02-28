/*
 *  AllocatorTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Memory/LinearAllocator.hpp>
#include <Bee/Core/Memory/PoolAllocator.hpp>
#include <Bee/Core/Memory/ThreadSafeLinearAllocator.hpp>
#include <Bee/Core/Containers/Array.hpp>
#include <Bee/Core/Concurrency.hpp>
#include <Bee/Core/Memory/ChunkAllocator.hpp>

#include <gtest/gtest.h>
#include <thread>


TEST(AllocatorTests, linear_allocator)
{
    bee::LinearAllocator allocator(128);
    bee::DynamicArray<void*> allocations;

    // check capacity minus headers
    for (size_t alloc_idx = 0; alloc_idx < allocator.capacity() / (sizeof(size_t) + 1); ++alloc_idx)
    {
        void* ptr = nullptr;
        ASSERT_NO_FATAL_FAILURE(ptr = allocator.allocate(1));
        allocations.push_back(ptr);
    }

    ASSERT_DEATH(allocator.allocate(23), "reached capacity");

    for (auto& alloc : allocations)
    {
        ASSERT_NO_FATAL_FAILURE(allocator.deallocate(alloc));
    }

    allocations.clear();
    allocator.reset();

    void* ptr = nullptr;
    ASSERT_NO_FATAL_FAILURE(ptr = allocator.allocate(allocator.max_allocation()));
    ASSERT_NO_FATAL_FAILURE(allocator.deallocate(ptr));

    allocator.reset();

    ptr = allocator.allocate(16);
    ASSERT_TRUE(allocator.is_valid(ptr));

    ASSERT_DEATH(allocator.reset(), "Not all allocations were deallocated");

    int value = 23;
    auto invalid_ptr = &value;
    ASSERT_FALSE(allocator.is_valid(invalid_ptr));
    invalid_ptr = nullptr;
    ASSERT_FALSE(allocator.is_valid(invalid_ptr));
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

TEST(AllocatorTests, ThreadSafeLinearAllocator)
{
    constexpr auto thread_count = 8;
    constexpr auto allocations_per_thread = 100;

    std::thread threads[thread_count];
    bee::ThreadSafeLinearAllocator allocator(1024 * 32);

    int* allocations[thread_count][allocations_per_thread];

    int index = 0;
    std::atomic_int32_t count(0);
    std::atomic_bool    flag(false);
    bee::Barrier barrier(bee::static_array_length(threads)); // include the main thread

    for (auto& thread : threads)
    {
        thread = std::thread([&, thread_index = index]()
        {
            for (int i = 0; i < allocations_per_thread; ++i)
            {
                allocations[thread_index][i] = BEE_NEW(&allocator, int)(i);
            }

            count.fetch_add(1);
            barrier.wait();

            while (!flag.load()) {}

            for (int i = 0; i < allocations_per_thread; ++i)
            {
                BEE_FREE(&allocator, allocations[thread_index][i]);
                allocations[thread_index][i] = nullptr;
            }

            barrier.wait();
        });
        ++index;
    }

    while (count.load() < bee::static_array_length(threads)) {};

    const auto single_alloc_size = sizeof(int);

    ASSERT_EQ(allocator.allocated_size(), single_alloc_size * allocations_per_thread * thread_count);

    for (auto per_thread : allocations)
    {
        for (int i = 0; i < allocations_per_thread; ++i)
        {
            ASSERT_EQ(i, *per_thread[i]);
        }
    }

    // Allow threads to deallocate and then unregister concurrently
    flag.store(true);

    for (auto& thread : threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    for (auto per_thread : allocations)
    {
        for (int i = 0; i < allocations_per_thread; ++i)
        {
            ASSERT_EQ(nullptr, per_thread[i]);
        }
    }

    ASSERT_EQ(allocator.allocated_size(), 0);
}

TEST(AllocatorTests, ChunkAllocator)
{
    struct TestData { int data[512]; };

    bee::ChunkAllocator allocator(bee::megabytes(4), 64, 1);

    bee::DynamicArray<TestData> array(&allocator);

    while (array.growth_rate() * sizeof(TestData) <= bee::megabytes(4))
    {
        ASSERT_NO_FATAL_FAILURE(array.push_back(TestData{}));
    }
}