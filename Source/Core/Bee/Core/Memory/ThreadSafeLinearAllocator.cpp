/*
 *  ThreadSafeLinearAllocator.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Memory/ThreadSafeLinearAllocator.hpp"
#include "Bee/Core/Thread.hpp"

#include <inttypes.h>

namespace bee {


static thread_local i32 g_local_index = -1;

/*
 *****************************
 *
 * PerThread - implementation
 *
 *****************************
 */
ThreadSafeLinearAllocator::PerThread::PerThread(const u64 new_thread_id, const size_t capacity, i32* local_index_ptr)
    : thread_id(new_thread_id),
      index_ptr(local_index_ptr)
{
    buffer = static_cast<u8*>(BEE_MALLOC_ALIGNED(system_allocator(), capacity, alignof(AllocHeader)));
}

ThreadSafeLinearAllocator::PerThread::~PerThread()
{
    if (buffer != nullptr)
    {
        BEE_FREE(system_allocator(), buffer);
    }

    buffer = nullptr;
    index_ptr = nullptr;
    thread_id = limits::max<u64>();
}

/*
 *********************************************
 *
 * ThreadSafeLinearAllocator - implementation
 *
 *********************************************
 */
ThreadSafeLinearAllocator::ThreadSafeLinearAllocator(const i32 max_threads, const size_t capacity)
    : ThreadSafeLinearAllocator(max_threads, capacity, nullptr)
{}

ThreadSafeLinearAllocator::ThreadSafeLinearAllocator(const i32 max_threads, const size_t capacity, Allocator* overflow_allocator)
    : max_threads_(max_threads),
      capacity_(capacity),
      overflow_(overflow_allocator)
{
    per_thread_ = static_cast<PerThread*>(BEE_MALLOC_ALIGNED(system_allocator(), sizeof(PerThread) * max_threads, alignof(PerThread)));
    per_thread_next_ = static_cast<i32*>(BEE_MALLOC_ALIGNED(system_allocator(), sizeof(i32) * max_threads, alignof(i32)));
    for (int t = 0; t < max_threads; ++t)
    {
        new (&per_thread_[t]) PerThread{};
        per_thread_next_[t] = t + 1;
    }
}

ThreadSafeLinearAllocator::ThreadSafeLinearAllocator(ThreadSafeLinearAllocator&& other) noexcept
{
    move_construct(other);
}

ThreadSafeLinearAllocator& ThreadSafeLinearAllocator::operator=(ThreadSafeLinearAllocator&& other) noexcept
{
    move_construct(other);
    return *this;
}

void ThreadSafeLinearAllocator::move_construct(ThreadSafeLinearAllocator& other) noexcept
{
    destroy();

    max_threads_ = other.max_threads_;
    capacity_ = other.capacity_;
    overflow_ = other.overflow_;
    allocated_size_.store(other.allocated_size_.load());
    next_thread_.store(other.next_thread_.load());
    per_thread_ = other.per_thread_;

    other.max_threads_ = 0;
    other.capacity_ = 0;
    other.overflow_ = nullptr;
    other.allocated_size_.store(0);
    other.next_thread_.store(0);
    other.per_thread_ = nullptr;
}

void ThreadSafeLinearAllocator::destroy()
{
    if (per_thread_ == nullptr)
    {
        return;
    }

    for (int t = 0; t < max_threads_; ++t)
    {
        if (per_thread_[t].is_valid())
        {
            *per_thread_[t].index_ptr = -1;
            destruct(&per_thread_[t]);
        }
    }

    BEE_FREE(system_allocator(), per_thread_);
    BEE_FREE(system_allocator(), per_thread_next_);
    per_thread_ = nullptr;
    per_thread_next_ = nullptr;
}

void ThreadSafeLinearAllocator::register_thread()
{
    BEE_ASSERT_F(g_local_index < 0, "ThreadSafeLinearAllocator: Thread is already registered");

    auto index = next_thread_.load();
    /*
     * It's possible this can lose a race with another thread registering - so we keep trying available indices until
     * we get one safely. Use strong because we don't want to accidentally not end up registering a desired
     * thread that came first
     */
    while (!next_thread_.compare_exchange_strong(index, per_thread_next_[index]))
    {
        index = next_thread_.load();
    }

    if (BEE_FAIL_F(index < max_threads_, "ThreadSafeLinearAllocator: maximum number of threads have been registered (%d)", max_threads_))
    {
        return;
    }

    per_thread_next_[index] = index;
    g_local_index = index;

    auto& local = per_thread_[index];
    new (&local) PerThread(current_thread::id(), capacity_, &g_local_index);
}

void ThreadSafeLinearAllocator::unregister_thread()
{
    BEE_ASSERT_F(g_local_index >= 0 && g_local_index < max_threads_, "ThreadSafeLinearAllocator: Thread is not registered");

    auto& local = per_thread_[g_local_index];
    destruct(&local);

    per_thread_next_[g_local_index] = next_thread_.exchange(per_thread_next_[g_local_index]);
    g_local_index = -1;
}

void* ThreadSafeLinearAllocator::allocate(const size_t size, const size_t alignment)
{
    if (BEE_FAIL_F(g_local_index >= 0, "ThreadSafeLinearAllocator: Current thread (%" PRIu64 ") is not registered", current_thread::id()))
    {
        return nullptr;
    }

    auto& local = per_thread_[g_local_index];
    
    const auto local_offset = local.offset.load(std::memory_order_acquire);
    const auto ptr_offset = round_up(local_offset + sizeof(AllocHeader), alignment);
    void* ptr = nullptr;
    bool is_overflow = false;

    if (ptr_offset + size > capacity_)
    {
        if (BEE_FAIL_F(overflow_ != nullptr, "ThreadSafeLinearAllocator: capacity reached and no overflow allocator was provided"))
        {
            return nullptr;
        }

        // Go into overflow memory if we've reached capacity on the thread local buffer
        auto overflow = BEE_MALLOC(overflow_, round_up(size, alignment) + sizeof(AllocHeader));
        ptr = align(reinterpret_cast<u8*>(overflow) + sizeof(AllocHeader), alignment);
        is_overflow = true;
    }
    else
    {
        auto expected_val = local_offset;
        // weak is fine here as calling code should be handling nullptr's from allocaters anyway
        if (!local.offset.compare_exchange_weak(expected_val, ptr_offset + size))
        {
            return nullptr; // failed a race with `reset()`
        }

        ptr = local.buffer + ptr_offset;
    }

    auto header = get_header(ptr);
    header->size = size + sizeof(AllocHeader);
    header->thread = g_local_index;
    header->is_overflow = is_overflow;

    allocated_size_.fetch_add(header->size, std::memory_order_release);

    return ptr;
}

void ThreadSafeLinearAllocator::deallocate(void *ptr)
{
    if (BEE_FAIL(is_valid(ptr)))
    {
        return;
    }

    auto header = get_header(ptr);
    const auto size = header->size; // copy the size to use later before deallocating it

    if (header->is_overflow)
    {
        BEE_FREE(overflow_, header);
    }

    const auto old_size = allocated_size_.fetch_sub(size, std::memory_order_release);
    if (BEE_FAIL_F(old_size >= size, "ThreadSafeLinearAllocator: Too much memory was deallocated"))
    {
        allocated_size_.store(0, std::memory_order_release);
    }
}

void* ThreadSafeLinearAllocator::reallocate(void* ptr, const size_t old_size, const size_t new_size, const size_t alignment)
{
    if (BEE_FAIL(is_valid(ptr)))
    {
        return nullptr;
    }

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    const auto validate_size = get_header(ptr)->size - sizeof(AllocHeader);
    BEE_ASSERT_F(old_size == validate_size, "ThreadSafeLinearAllocator: Invalid `old_size` given to `reallocate` for that pointer");
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    auto new_memory = allocate(new_size, alignment);
    if (new_memory != nullptr)
    {
        memcpy(new_memory, ptr, old_size < new_size ? old_size : new_size);
        deallocate(ptr);
    }
    return new_memory;
}

bool ThreadSafeLinearAllocator::is_valid(const void* ptr) const
{
    if (BEE_FAIL_F(g_local_index >= 0, "ThreadSafeLinearAllocator: Current thread (%" PRIu64 ") is not registered", current_thread::id()))
    {
        return false;
    }

    if (ptr == nullptr)
    {
        return false;
    }

    auto header = get_header(ptr);

    if (header->thread < 0 || header->thread >= max_threads_)
    {
        return false;
    }

    if (header->is_overflow)
    {
        return true;
    }

    auto& local = per_thread_[header->thread];
    return ptr >= local.buffer && ptr < local.buffer + capacity_;
}

void ThreadSafeLinearAllocator::reset()
{
#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    const auto allocated_size_before_reset = allocated_size_.load(std::memory_order_acquire);
    BEE_ASSERT_F(allocated_size_before_reset == 0, "ThreadSafeLinearAllocator: not all allocations were deallocated before calling `reset` - this indicates a memory leak");
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    const auto thread_count = next_thread_.load(std::memory_order_relaxed) - 1;
    for (int t = 0; t < thread_count; ++t)
    {
        // this operation is the authoritative one over any `allocate` operations
        // (`allocate` does a CAS before allocating to check for races)
        per_thread_[t].offset.store(0, std::memory_order_release);
    }
}

const u8* ThreadSafeLinearAllocator::data() const
{
    if (BEE_FAIL_F(g_local_index >= 0, "ThreadSafeLinearAllocator: Current thread (%" PRIu64 ") is not registered", current_thread::id()))
    {
        return nullptr;
    }

    return per_thread_[g_local_index].buffer;
}

size_t ThreadSafeLinearAllocator::offset() const
{
    if (BEE_FAIL_F(g_local_index >= 0, "ThreadSafeLinearAllocator: Current thread (%" PRIu64 ") is not registered", current_thread::id()))
    {
        return 0;
    }

    return per_thread_[g_local_index].offset.load(std::memory_order_relaxed);
}


} // namespace bee