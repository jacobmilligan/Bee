/*
 *  ChunkAllocatorTest.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Main.hpp"
#include "Bee/Core/Memory/ChunkAllocator.hpp"

int bee_main(int argc, char** argv)
{
    struct TestData { int data[512]; };

    bee::ChunkAllocator allocator(bee::megabytes(16), 64, 1);

    bee::DynamicArray<TestData> array(&allocator);

    const int count = bee::megabytes(2) / sizeof(TestData);
    for (int i = 0; i < count - 1; ++i)
    {
        array.push_back(TestData{});
    }

    return EXIT_SUCCESS;
}