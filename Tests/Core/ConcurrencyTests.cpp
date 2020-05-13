/*
 *  ConcurrencyTests.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Concurrency.hpp>
#include <Bee/Core/Random.hpp>
#include <Bee/Core/Containers/Array.hpp>

#include <gtest/gtest.h>

#include <thread>

TEST(ConcurrencyTests, atomic_ptr_stack_works_as_stack)
{
    bee::AtomicStack stack;
    bee::AtomicNode nodes[5];

    // ensure empty stack
    ASSERT_EQ(stack.pop(), nullptr);
    ASSERT_TRUE(stack.empty());

    int val = 1;
    for (auto& node : nodes)
    {
        node.data[0] = new int(val);
        val *= 2;
        stack.push(&node);
    }

    // ensure stack pops in reverse order
    for (int i = bee::static_array_length(nodes) - 1; i >= 0 ; --i)
    {
        auto node = stack.pop();
        ASSERT_EQ(node->version, 1);
        ASSERT_EQ(node, &nodes[i]);
        ASSERT_EQ(node->data, nodes[i].data);
    }

    for (auto& node : nodes)
    {
        delete static_cast<int*>(node.data[0]);
    }
}

TEST(ConcurrencyTests, atomic_ptr_stack_stress_test)
{
    constexpr int node_count = 100000;
    constexpr int thread_count = 64;

    bee::AtomicStack stack{};
    auto nodes = bee::FixedArray<bee::AtomicNode>::with_size(node_count);
    int results[node_count] { 0 };
    std::thread threads[thread_count];

    int index = 0;
    for (auto& node : nodes)
    {
        node.data[0] = new int(index);
        results[index] = 0;
        stack.push(&node);
        ++index;
    }

    for (auto& t : threads)
    {
        t = std::thread([&]()
        {
            bee::RandomGenerator<bee::Xorshift> random;

            for (int i = 0; i < node_count; ++i)
            {
                auto node = stack.pop();
                if (node != nullptr)
                {
                    bee::current_thread::sleep(random.random_range(10, 1000));
                    stack.push(node);
                }
            }
        });
    }

    for (auto& t : threads)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    int count = 0;
    while (!stack.empty())
    {
        auto node = stack.pop();
        if (node == nullptr)
        {
            continue;
        }
        const auto result_index = *static_cast<int*>(node->data[0]);
        ++results[result_index];
        ++count;
    }

    ASSERT_EQ(count, node_count);

    index = 0;
    for (auto& result : results)
    {
        ASSERT_EQ(result, 1) << "index: " << index;
        ++index;
    }

    for (auto& node : nodes)
    {
        delete static_cast<int*>(node.data[0]);
    }
}