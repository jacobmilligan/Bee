//
//  RingBufferTests.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 12/09/2018
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#include <Bee/Core/Containers/RingBuffer.hpp>

#include <gtest/gtest.h>

TEST(RingBufferTests, test_ring_buffer_wraps)
{
    // 2 less bytes than needed to write 16 ints to the buffer
    bee::RingBuffer buffer(62);

    for (int i = 0; i < 15; ++i) {
        auto one_based_idx = i + 1;
        auto write_success = buffer.write(&i, sizeof(int));
        ASSERT_TRUE(write_success);
        ASSERT_EQ(buffer.size(), sizeof(int) * one_based_idx);
        ASSERT_EQ(buffer.write_position(), sizeof(int) * one_based_idx);
        ASSERT_EQ(buffer.size(), sizeof(int) * one_based_idx);
    }

    // read a value to free up 4 bytes at end of buffer
    int val = -1;
    auto read_success = buffer.read(&val, sizeof(int));
    ASSERT_TRUE(read_success);
    ASSERT_EQ(val, 0);

    val = 23;
    auto write_success = buffer.write(&val, sizeof(int));
    ASSERT_TRUE(write_success);

    // Read the last 15 values of the original write
    for (int i = 0; i < 14; ++i) {
        val = -1;
        read_success = buffer.read(&val, sizeof(int));
        ASSERT_TRUE(read_success);
        ASSERT_EQ(val, i + 1);
    }

    ASSERT_EQ(buffer.read_position(), sizeof(int) * 15);
    // value would have been split in half if correctly wrapped
    ASSERT_EQ(buffer.write_position(), sizeof(int) / 2);

    // Read the last, wrapped value
    val = -1;
    read_success = buffer.read(&val, sizeof(int));
    ASSERT_TRUE(read_success);
    ASSERT_EQ(val, 23);
    ASSERT_EQ(buffer.read_position(), sizeof(int) / 2);
}

TEST(RingBufferTests, test_ring_buffer_resets_correctly)
{
    bee::RingBuffer buffer(32);
    int val = 1;
    buffer.write(&val, sizeof(int));
    ASSERT_EQ(buffer.size(), sizeof(int));
    buffer.read(&val, sizeof(int));
    ASSERT_EQ(buffer.write_position(), sizeof(int));
    ASSERT_EQ(buffer.read_position(), sizeof(int));
    buffer.reset();
    ASSERT_EQ(buffer.write_position(), buffer.read_position());
    ASSERT_EQ(buffer.write_position(), 0);
    ASSERT_EQ(buffer.size(), 0);
}

TEST(RingBufferTests, test_ring_buffer_fills_and_empties_correctly)
{
    bee::RingBuffer buffer(32);
    for (int i = 0; i < 32 / sizeof(int); ++i) {
        buffer.write(&i, sizeof(int));
    }

    int val[] = { -1, -1 };
    auto write_success = buffer.write(val, sizeof(int) * 2);
    ASSERT_FALSE(write_success);
}

// Read empty
// write full
//