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

/*
 *********************************************
 *
 * ThreadSafeLinearAllocator - implementation
 *
 *********************************************
 */
ThreadSafeLinearAllocator::ThreadSafeLinearAllocator(const size_t capacity)
    : ThreadSafeLinearAllocator(capacity, nullptr)
{}

ThreadSafeLinearAllocator::ThreadSafeLinearAllocator(const size_t capacity, Allocator* overflow_allocator)
    : capacity_(capacity),
      overflow_(overflow_allocator)
{
    buffer_ = static_cast<u8*>(BEE_MALLOC_ALIGNED(system_allocator(), capacity, alignof(Header)));
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

    capacity_ = other.capacity_;
    allocated_size_.store(other.allocated_size_.load());
    offset_.store(other.offset_.load());
    buffer_ = other.buffer_;
    overflow_ = other.overflow_;
    overflow_stack_ = std::move(other.overflow_stack_);

    other.capacity_ = 0;
    other.allocated_size_.store(0);
    other.offset_.store(0);
    other.buffer_ = nullptr;
    other.overflow_ = nullptr;
}

void ThreadSafeLinearAllocator::destroy()
{
    reset();

    if (buffer_ != nullptr)
    {
        BEE_FREE(system_allocator(), buffer_);
    }

    buffer_ = nullptr;
}

AtomicNode* ThreadSafeLinearAllocator::allocate_overflow_node(const size_t size, const size_t alignment)
{
    auto ptr = BEE_MALLOC_ALIGNED(overflow_, size + sizeof(Header) + sizeof(AtomicNode), alignment);
    auto node = static_cast<AtomicNode*>(ptr);
    new (node) AtomicNode{};
    node->data[0] = reinterpret_cast<u8*>(ptr) + sizeof(AtomicNode) + sizeof(Header);
    return node;
}


void* ThreadSafeLinearAllocator::allocate(const size_t size, const size_t alignment)
{
    const auto alloc_size = round_up(size + sizeof(Header), alignment);
    const auto old_offset = offset_.fetch_add(alloc_size, std::memory_order_acquire);
    void* ptr = nullptr;
    bool is_overflow = false;

    if (old_offset + alloc_size < capacity_)
    {
        ptr = buffer_ + old_offset + sizeof(Header);
    }
    else
    {
        // reduce the offset by the overflowed allocation size
        offset_.fetch_sub(alloc_size, std::memory_order_release);

        if (BEE_FAIL_F(overflow_ != nullptr, "ThreadSafeLinearAllocator: capacity reached and no overflow allocator was provided"))
        {
            return nullptr;
        }

        // Go into overflow memory if we've reached capacity on the thread local buffer
        auto overflow_node = allocate_overflow_node(size, alignment);
        ptr = overflow_node->data;

        is_overflow = true;
    }

    auto header = get_header(ptr);
    header->size = size;
    header->is_overflow = is_overflow;

    allocated_size_.fetch_add(size, std::memory_order_release);

    return ptr;
}

void ThreadSafeLinearAllocator::deallocate(void *ptr)
{
    const auto header = get_header(ptr);

    if (BEE_FAIL(is_valid(ptr)))
    {
        return;
    }

    const auto size = header->size;
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
    const auto validate_size = get_header(ptr)->size;
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

bool ThreadSafeLinearAllocator::is_valid(const Header* header) const
{
    if (header == nullptr)
    {
        return false;
    }

    if (header->is_overflow)
    {
        return overflow_->is_valid(header);
    }

    auto ptr = static_cast<const void*>(header);
    return ptr >= buffer_ && ptr < buffer_ + capacity_;
}

bool ThreadSafeLinearAllocator::is_valid(const void* ptr) const
{
    return is_valid(get_header(ptr));
}

void ThreadSafeLinearAllocator::reset()
{
#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    const auto allocated_size_before_reset = allocated_size_.load(std::memory_order_acquire);
    if (allocated_size_before_reset != 0)
    {
        memory_tracker::log_tracked_allocations(LogVerbosity::info);
    }
    BEE_ASSERT_F(allocated_size_before_reset == 0, "ThreadSafeLinearAllocator: not all allocations were deallocated before calling `reset` - this indicates a memory leak");
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    while (!overflow_stack_.empty())
    {
        auto overflow_alloc = overflow_stack_.pop();
        if (overflow_alloc != nullptr)
        {
            BEE_FREE(overflow_, overflow_alloc);
        }
    }

    offset_.store(0, std::memory_order_release);
}

size_t ThreadSafeLinearAllocator::offset() const
{
    return offset_.load(std::memory_order_relaxed);
}


} // namespace bee