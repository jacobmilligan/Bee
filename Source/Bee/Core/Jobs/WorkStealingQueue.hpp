/*
 *  WorkStealingQueue.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Noncopyable.hpp"
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Containers/Array.hpp"

#include <atomic>

namespace bee {


class Job;

/*
 * source: 'Dynamic Circular Work-Stealing Deque', Chase D. & Lev Y. 2005
 * http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.170.1097&rep=rep1&type=pdf
 */
class WorkStealingQueue : Noncopyable
{
public:
    WorkStealingQueue() = default;

    explicit WorkStealingQueue(const i32 job_buffer_capacity, Allocator* allocator = system_allocator());

    WorkStealingQueue(WorkStealingQueue&& other) noexcept;

    ~WorkStealingQueue();

    WorkStealingQueue& operator=(WorkStealingQueue&& other) noexcept;

    void push(Job* job);

    Job* pop();

    Job* steal();

    inline bool empty() const
    {
        const auto top = top_idx_.load(std::memory_order_relaxed);
        const auto bottom = bottom_idx_.load(std::memory_order_relaxed);
        return bottom <= top;
    }

    inline u32 capacity() const
    {
        return job_buffer_capacity_;
    }
private:
    Allocator*          allocator_ { nullptr };
    Job**               job_buffer_ { nullptr };
    u32                 job_buffer_capacity_ { 0 };
    u32                 job_buffer_mask_ { 0 };
    std::atomic_int32_t bottom_idx_ { 0 }; // incremented on every `push_bottom`
    std::atomic_int32_t top_idx_ { 0 }; // incremented on every `steal`

    void destroy();

    void move_construct(WorkStealingQueue&& other) noexcept;
};


} // namespace bee