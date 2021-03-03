/*
 *  ThreadSafeLinearAllocator.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/Concurrency.hpp"

namespace bee {


/*
 **************************************************************
 *
 * # ThreadSafeLinearAllocator
 *
 * Guarantees thread-safety by setting up one allocator
 * per registered thread, only locking when migrating
 * allocations across these threads
 * (i.e. when reallocating from a different thread than the
 * one it was allocated on)
 *
 **************************************************************
 */
class BEE_CORE_API ThreadSafeLinearAllocator final : public Allocator
{
private:
    struct Header
    {
        size_t      size { 0 };
        Allocator*  overflow_allocator { nullptr };
    };
public:
    static constexpr size_t min_allocation = sizeof(Header);

    using Allocator::allocate;

    BEE_ALLOCATOR_DO_NOT_TRACK

    ThreadSafeLinearAllocator() = default;

    explicit ThreadSafeLinearAllocator(const size_t capacity);

    ThreadSafeLinearAllocator(const size_t capacity, Allocator* overflow_allocator);

    ThreadSafeLinearAllocator(ThreadSafeLinearAllocator&& other) noexcept;

    ~ThreadSafeLinearAllocator() override
    {
        destroy();
    }

    ThreadSafeLinearAllocator& operator=(ThreadSafeLinearAllocator&& other) noexcept;

    void destroy();

    void* allocate(const size_t size, const size_t alignment) override;

    void deallocate(void* ptr) override;

    void* reallocate(void* ptr, const size_t old_size, const size_t new_size, const size_t alignment) override;

    bool is_valid(const void* ptr) const override;

    void reset();

    size_t offset() const;

    inline size_t capacity() const
    {
        return capacity_;
    }

    inline size_t allocated_size() const
    {
        return allocated_size_.load(std::memory_order_relaxed);
    }

    inline size_t max_allocation() const
    {
        return capacity_ - sizeof(size_t);
    }

private:
    size_t              capacity_ { 0 };
    std::atomic_size_t  allocated_size_ { 0 };
    std::atomic_size_t  offset_ { 0 };
    u8*                 buffer_ { nullptr };

    Allocator*          overflow_ { nullptr };
    AtomicStack         overflow_stack_;

    void move_construct(ThreadSafeLinearAllocator& other) noexcept;

    static inline Header* get_header(void* ptr)
    {
        return reinterpret_cast<Header*>(static_cast<u8*>(ptr) - sizeof(Header));
    }

    static inline const Header* get_header(const void* ptr)
    {
        return reinterpret_cast<const Header*>(static_cast<const u8*>(ptr) - sizeof(Header));
    }

    bool is_valid(const Header* header) const;

    AtomicNode* allocate_overflow_node(const size_t size, const size_t alignment);
};


} // namespace bee