/*
 *  HashMapTests.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Core/Reflection.hpp>
#include <Bee/Core/Containers/HashMap.hpp>
#include <Bee/Core/Containers/Array.hpp>

#include <GTest.hpp>

#include <random>


class HashMapTests : public ::testing::Test {
public:
    HashMapTests()
        : keys_(num_iterations),
          values_(num_iterations)
    {}

protected:
    static constexpr int num_iterations = 100000;

    bee::DynamicArray<int> keys_;
    bee::DynamicArray<int> values_;

    void SetUp() override
    {
        for (int i = 0; i < num_iterations; ++i) {
            keys_.push_back(i);
        }

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(keys_.begin(), keys_.end(), g);

        for (int i = 0; i < num_iterations; ++i) {
            values_.push_back(g());
        }
    }
};

TEST_F(HashMapTests, insertion_and_lookup)
{
    bee::DynamicHashMap<int, int> map;
    for (int i = 0; i < num_iterations; ++i) {
        map.insert(keys_[i], values_[i]);
    }

    for (int i = 0; i < num_iterations; ++i) {
        auto val = map.find(keys_[i]);
        ASSERT_EQ(val->value, values_[i]);
    }
}

TEST_F(HashMapTests, stress_test)
{
    constexpr auto stress_iterations = num_iterations / 2;
    bee::DynamicHashMap<int, int> map;
    for (int i = 0; i < stress_iterations - 1; ++i) {
        auto kv = map.insert(keys_[i], values_[i]);
        ASSERT_EQ(kv->key, keys_[i]);
        ASSERT_EQ(kv->value, values_[i]);
    }

    for (int j = 0; j < stress_iterations - 1; j += 2) {
        // remove every first key and replace with something from the other half of the key/val table
        ASSERT_TRUE(map.erase(keys_[j]));
        auto kv = map.insert(keys_[stress_iterations + j], values_[stress_iterations + j]);
        ASSERT_EQ(kv->key, keys_[stress_iterations + j]);
        ASSERT_EQ(kv->value, values_[stress_iterations + j]);
    }

    for (int k = 0; k < stress_iterations - 2; k += 2) {
        auto keyval1 = map.find(keys_[k]);
        auto keyval2 = map.find(keys_[k + 1]);
        ASSERT_EQ(keyval1, nullptr);
        ASSERT_EQ(keyval2->value, values_[k + 1]);
    }
}

TEST_F(HashMapTests, map_functions_correctly_after_clear)
{
    bee::DynamicHashMap<int, int> map;
    for (int i = 0; i < num_iterations; ++i) {
        auto kv = map.insert(keys_[i], values_[i]);
        ASSERT_EQ(kv->key, keys_[i]);
        ASSERT_EQ(kv->value, values_[i]);
    }

    map.clear();

    for (int i = 0; i < num_iterations; ++i) {
        auto kv = map.find(keys_[i]);
        ASSERT_EQ(kv, nullptr);
    }

    for (int i = 0; i < num_iterations; ++i) {
        auto kv = map.insert(keys_[i], values_[i]);
        ASSERT_EQ(kv->key, keys_[i]);
        ASSERT_EQ(kv->value, values_[i]);
    }
}

TEST_F(HashMapTests, rehashing_works_from_client_code)
{
    bee::DynamicHashMap<int, int> map;
    for (int i = 0; i < num_iterations / 2; ++i) {
        auto kv = map.insert(keys_[i], values_[i]);
        ASSERT_EQ(kv->key, keys_[i]);
        ASSERT_EQ(kv->value, values_[i]);
    }

    map.rehash(bee::math::to_next_pow2(num_iterations));

    for (int i = 0; i < num_iterations / 2; ++i) {
        auto kv = map.find(keys_[i]);
        ASSERT_EQ(kv->key, keys_[i]);
        ASSERT_EQ(kv->value, values_[i]);
    }

    ASSERT_DEATH(map.rehash(3), "new capacity must be a power of 2");
}

TEST_F(HashMapTests, fixed_hash_map)
{
    bee::FixedHashMap<int, int> map(32);
    for (int i = 0; i < 32; ++i) {
        ASSERT_NO_FATAL_FAILURE(map.insert(i, i));
    }
    ASSERT_DEATH(map.insert(256, 1), "HashMap: unable to find a free slot for insertion");
}

TEST_F(HashMapTests, duplicate_key)
{
    bee::FixedHashMap<int, int> map(32);
    map.insert(12, 50);

    ASSERT_EQ(map.size(), 1);
    ASSERT_DEATH(map.insert(12, 100), "Check failed");
}

TEST_F(HashMapTests, subscript_operator)
{
    bee::FixedHashMap<int, int> map(32);
    map[12] = 50;
    ASSERT_EQ(map[12], 50);
    map[12] = 123;
    ASSERT_EQ(map[12], 123);
    ASSERT_EQ(map.size(), 1);
}
