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

    bee::ChunkAllocator allocator(bee::megabytes(4), 64, 1);

    bee::DynamicArray<TestData> array(&allocator);

    while (array.growth_rate() * sizeof(TestData) <= bee::megabytes(4))
    {
        array.push_back(TestData{});
    }

    return EXIT_SUCCESS;
}