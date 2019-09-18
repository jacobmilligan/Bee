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
struct Worker;

class BEE_CORE_API  Job
{
public:
    explicit Job();

    Job(Job&& other) noexcept;

    Job& operator=(Job&& other) noexcept;

    virtual ~Job();

    virtual void execute() = 0;

    void complete();

    void add_dependency(Job* dependency);

    i32 dependency_count() const;

    bool has_dependencies() const;

    Job* parent() const;

    i32 owning_worker_id() const;
private:
    i32                 owning_worker_ { 0 };
    std::atomic_int32_t dependency_count_;
    std::atomic<Job*>   parent_;
    char                pad_[32 - sizeof(owning_worker_) - sizeof(dependency_count_) - sizeof(parent_)];

    void move_construct(Job& other) noexcept;

    void signal_completed_dependency();
};

template <typename FunctionType>
class FunctionJob : public Job
{
public:
    explicit FunctionJob(const FunctionType& function)
        : function_(function)
    {}

    void execute() override
    {
        function_();
    }
private:
    FunctionType function_;
};


class EmptyJob : public Job
{
public:
    using Job::Job;

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
