/*
 *  JobTypes.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/Functional.hpp"

#include <atomic>
#include <mutex>

namespace bee {


class Allocator;

class Job
{
public:
    explicit Job(const i32 owning_worker)
        : dependency_count_(1),
          owning_worker_(owning_worker),
          parent_(nullptr)
    {}

    Job(Job&& other) noexcept
    {
        move_construct(other);
    }

    Job& operator=(Job&& other) noexcept
    {
        move_construct(other);
        return *this;
    }

    virtual ~Job()
    {
        owning_worker_ = -1;
        parent_.store(nullptr, std::memory_order_release);
    }

    virtual void execute() = 0;

    void complete()
    {
        execute();

        if (parent() != nullptr)
        {
            parent()->signal_completed_dependency();
        }
        signal_completed_dependency();
    }

    void add_dependency(Job* dependency)
    {
        if (BEE_FAIL(dependency != this))
        {
            return;
        }

        if (BEE_FAIL(dependency->parent() == nullptr))
        {
            return;
        }

        dependency_count_.fetch_add(1, std::memory_order_release);
        dependency->parent_.store(this, std::memory_order_release);
    }

    inline i32 dependency_count() const
    {
        return dependency_count_.load(std::memory_order_acquire);
    }

    inline bool has_dependencies() const
    {
        return dependency_count() > 0;
    }

    inline Job* parent() const
    {
        return parent_.load(std::memory_order_acquire);
    }

    inline i32 owning_worker_id() const
    {
        return owning_worker_;
    }
private:
    i32                 owning_worker_ { 0 };
    std::atomic_int32_t dependency_count_;
    std::atomic<Job*>   parent_;
    char                pad_[32 - sizeof(owning_worker_) - sizeof(dependency_count_) - sizeof(parent_)];

    void move_construct(Job& other) noexcept
    {
        dependency_count_.store(other.dependency_count_, std::memory_order_acq_rel);
        parent_.store(other.parent_, std::memory_order_acq_rel);
        owning_worker_ = other.owning_worker_;

        other.dependency_count_.store(0, std::memory_order_acq_rel);
        other.parent_.store(nullptr, std::memory_order_acq_rel);
        other.owning_worker_ = -1;
    }

    inline void signal_completed_dependency()
    {
        dependency_count_.fetch_sub(1, std::memory_order_acq_rel);
    }
};

template <typename FunctionType>
class FunctionJob : public Job
{
public:
    FunctionJob(const i32 owning_worker, const FunctionType& function)
        : Job(owning_worker),
          function_(function)
    {}

    void execute() override
    {
        function_();
    }
private:
    FunctionType function_;
};


class EmptyOperationJob : public Job
{
public:
    explicit EmptyOperationJob(const i32 owning_worker)
        : Job(owning_worker)
    {}

    void execute() override
    {
        // no-op
    }
};

template <typename FunctionType>
inline void parallel_for_single_batch(const i32 range_begin, const i32 range_end, const FunctionType& function)
{
    for (int i = range_begin; i < range_end; ++i)
    {
        function(i);
    }
}


} // namespace bee
