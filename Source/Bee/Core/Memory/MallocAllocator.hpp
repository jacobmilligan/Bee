/*
 *  MallocAllocator.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Memory/Memory.hpp"
#include "Bee/Core/Error.hpp"

namespace bee {


class BEE_CORE_API MallocAllocator : public Allocator {
public:
    MallocAllocator() = default;

    bool is_valid(const void* ptr) const override
    {
        return ptr != nullptr;
    }

    size_t allocation_size(const void* ptr) const;

    void* allocate(size_t size, size_t alignment) override;

    void* allocate(size_t size) override;

    void deallocate(void* ptr) override;

    void* reallocate(void* ptr, size_t old_size, size_t new_size, size_t alignment) override;
};


} // namespace bee