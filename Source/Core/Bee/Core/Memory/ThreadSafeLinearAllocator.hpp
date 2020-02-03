/*
 *  ThreadSafeLinearAllocator.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/Allocator.hpp"

#include <atomic>

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
    struct AllocHeader
    {
        size_t  size { 0 };
        i32     thread { -1 };
        bool    is_overflow { false };
    };
public:
    static constexpr size_t min_allocation = sizeof(AllocHeader);

    using Allocator::allocate;

    BEE_ALLOCATOR_DO_NOT_TRACK

    ThreadSafeLinearAllocator() = default;

    explicit ThreadSafeLinearAllocator(const i32 max_threads, const size_t capacity);

    ThreadSafeLinearAllocator(const i32 max_threads, const size_t capacity, Allocator* overflow_allocator);

    ThreadSafeLinearAllocator(ThreadSafeLinearAllocator&& other) noexcept;

    ~ThreadSafeLinearAllocator() override
    {
        destroy();
    }

    ThreadSafeLinearAllocator& operator=(ThreadSafeLinearAllocator&& other) noexcept;

    void register_thread();

    void unregister_thread();

    void destroy();

    void* allocate(const size_t size, const size_t alignment) override;

    void deallocate(void* ptr) override;

    void* reallocate(void* ptr, const size_t old_size, const size_t new_size, const size_t alignment) override;

    bool is_valid(const void* ptr) const override;

    void reset();

    const u8* data() const;

    size_t offset() const;

    inline size_t capacity_per_thread() const
    {
        return capacity_;
    }

    inline i32 max_threads() const
    {
        return max_threads_;
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
    // Per-thread data
    struct PerThread
    {
        u64                 thread_id { limits::max<u64>() };
        i32*                index_ptr { nullptr };
        std::atomic_size_t  offset { 0 };
        u8*                 buffer { nullptr };

        PerThread() = default;

        explicit PerThread(const u64 new_thread_id, const size_t capacity, i32* local_index_ptr);

        ~PerThread();

        inline bool is_valid() const
        {
            return index_ptr != nullptr && buffer != nullptr && thread_id != limits::max<u64>();
        }
    };

    i32                 max_threads_ { -1 };
    size_t              capacity_ { 0 };
    Allocator*          overflow_ { nullptr };
    std::atomic_size_t  allocated_size_ { 0 };
    std::atomic_int32_t next_thread_ { 0 };
    PerThread*          per_thread_ { nullptr };
    i32*                per_thread_next_ { nullptr };

    void move_construct(ThreadSafeLinearAllocator& other) noexcept;

    static inline AllocHeader* get_header(void* ptr)
    {
        return reinterpret_cast<AllocHeader*>(static_cast<u8*>(ptr) - sizeof(AllocHeader));
    }

    static inline const AllocHeader* get_header(const void* ptr)
    {
        return reinterpret_cast<const AllocHeader*>(static_cast<const u8*>(ptr) - sizeof(AllocHeader));
    }
};


} // namespace bee