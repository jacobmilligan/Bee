/*
 *  WorkStealingQueue.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Jobs/WorkStealingQueue.hpp"
#include "Bee/Core/Jobs/JobTypes.hpp"

namespace bee {


WorkStealingQueue::WorkStealingQueue(const i32 job_buffer_capacity, Allocator* allocator)
    : allocator_(allocator),
      job_buffer_capacity_(sign_cast<u32>(job_buffer_capacity)),
      bottom_idx_(0),
      top_idx_(0)
{
    BEE_ASSERT_F(
        job_buffer_capacity_ >= 2 && math::is_power_of_two(job_buffer_capacity_),
        "WorkStealingQueue<T>: capacity must be a power of two and >= 2"
    );
    job_buffer_mask_ = job_buffer_capacity_ - 1u;
    job_buffer_ = static_cast<Job**>(BEE_MALLOC(allocator_, sizeof(Job*) * job_buffer_capacity));
}

WorkStealingQueue::WorkStealingQueue(WorkStealingQueue&& other) noexcept
{
    move_construct(std::forward<WorkStealingQueue>(other));
}

WorkStealingQueue::~WorkStealingQueue()
{
    destroy();
}

WorkStealingQueue& WorkStealingQueue::operator=(WorkStealingQueue&& other) noexcept
{
    move_construct(std::forward<WorkStealingQueue>(other));
    return *this;
}

void WorkStealingQueue::destroy()
{
    if (allocator_ == nullptr || job_buffer_ == nullptr)
    {
        return;
    }

    BEE_FREE(allocator_, job_buffer_);
    allocator_ = nullptr;
    job_buffer_ = nullptr;
}

void WorkStealingQueue::move_construct(WorkStealingQueue&& other) noexcept
{
    destroy();
    allocator_ = other.allocator_;
    job_buffer_ = other.job_buffer_;
    job_buffer_capacity_ = other.job_buffer_capacity_;
    job_buffer_mask_ = other.job_buffer_mask_;
    bottom_idx_.store(other.bottom_idx_, std::memory_order_seq_cst);
    top_idx_.store(other.top_idx_, std::memory_order_seq_cst);

    other.allocator_ = nullptr;
    other.job_buffer_ = nullptr;
    other.job_buffer_capacity_ = 0;
    other.job_buffer_mask_ = 0;
}

void WorkStealingQueue::push(Job* job)
{
    const auto bottom = bottom_idx_.load(std::memory_order_relaxed);

    // implements the `put` operation
    job_buffer_[bottom & job_buffer_mask_] = job;

    // use just a compiler fence here - stop reads before the job has been written
    std::atomic_signal_fence(std::memory_order_release);

    bottom_idx_.store(bottom + 1, std::memory_order_relaxed);
}

Job* WorkStealingQueue::pop()
{
    const auto bottom = bottom_idx_.fetch_sub(1, std::memory_order_relaxed) - 1;

    std::atomic_thread_fence(std::memory_order_seq_cst);

    const auto top = top_idx_.load(std::memory_order_relaxed);

    if (top > bottom)
    {
        // empty so clear bottom to empty state, i.e. bottom == top
        bottom_idx_.store(bottom + 1, std::memory_order_relaxed);
        return nullptr;
    }

    auto job = job_buffer_[bottom & job_buffer_mask_];
    if (top != bottom)
    {
        // non-empty queue so just return the job - nothing fancy here
        return job;
    }

    auto expected_top = top;
    // If we're popping the last item in the queue we need to check if we fail any races with a `steal` operation
    if (!top_idx_.compare_exchange_strong(expected_top, expected_top + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
    {
        // failed race
        job = nullptr;
    }

    bottom_idx_.store(top + 1, std::memory_order_relaxed);
    return job;
}

Job* WorkStealingQueue::steal()
{
    const auto top = top_idx_.load(std::memory_order_acquire);

    std::atomic_signal_fence(std::memory_order_seq_cst);

    const auto bottom = bottom_idx_.load(std::memory_order_acquire);

    if (top >= bottom)
    {
        // empty queue
        return nullptr;
    }

    // implements the `get` operation
    auto item = job_buffer_[top & job_buffer_mask_];

    auto expected_top = top;
    // check for races with a `pop` operation and if successful increment the `top`
    if (!top_idx_.compare_exchange_strong(expected_top, expected_top + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
    {
        // Failed a race with a `pop` operation
        return nullptr;
    }

    job_buffer_[top & job_buffer_mask_] = nullptr;
    return item;
}


} // namespace bee
