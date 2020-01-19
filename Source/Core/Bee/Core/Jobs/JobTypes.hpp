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
class Job;

class JobGroup
{
public:
    void add_job(Job* job);

    i32 pending_count();

    bool has_pending_jobs();

    void signal(Job* job);
private:
    std::atomic_int32_t pending_count_ { 0 };
};

class BEE_CORE_API  Job
{
public:
    explicit Job();

    Job(Job&& other) noexcept;

    Job& operator=(Job&& other) noexcept;

    virtual ~Job();

    virtual void execute() = 0;

    void complete();

    void set_group(JobGroup* group);

    JobGroup* parent() const;

    i32 owning_worker_id() const;
private:
    i32                     owning_worker_ { 0 };
    std::atomic<JobGroup*>  parent_;

    void move_construct(Job& other) noexcept;
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
