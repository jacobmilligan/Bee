/*
 *  VMAllocator.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Error.hpp"
#include "Bee/Core/Memory/Allocator.hpp"

namespace bee {


class BEE_API VMAllocator : public Allocator {
public:
    using Allocator::Allocator;
    using Allocator::allocate;

    static const size_t page_size;

    VMAllocator() = default;

    VMAllocator(VMAllocator&& other) noexcept = default;

    VMAllocator& operator=(VMAllocator&& other) noexcept = default;

    bool is_valid(const void* ptr) const override
    {
        return ptr != nullptr;
    }

    /// size refers to the number of pages to allocate
    void* allocate(size_t size, size_t alignment) override;

    void deallocate(void* ptr) override
    {
        BEE_UNREACHABLE(
            "VMAllocator::deallocate is only implemented where the size of the deallocation is explicitly given"
        );
    }

    void deallocate(void* ptr, size_t size);

    void* reallocate(void* ptr, size_t old_size, size_t new_size, size_t alignment) override;
};


} // namespace bee