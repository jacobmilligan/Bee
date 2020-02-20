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
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Containers/Array.hpp"

#include <atomic>
#include <mutex>

namespace bee {


class Allocator;
struct Worker;
class Job;

class BEE_CORE_API JobGroup
{
public:
    explicit JobGroup(Allocator* allocator = system_allocator());

    ~JobGroup();

    void add_job(Job* job);

    void add_dependency(JobGroup* child_group);

    i32 pending_count();

    i32 dependency_count();

    bool has_pending_jobs();

    bool has_dependencies();

    void signal(Job* job);
private:
    std::atomic_int32_t     pending_count_ { 0 };
    std::atomic_int32_t     dependency_count_ { 0 };
    ReaderWriterMutex       parents_mutex_;
    DynamicArray<JobGroup*> parents_;
};

class BEE_CORE_API Job
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
    std::atomic_int32_t     owning_worker_ { 0 };
    std::atomic<JobGroup*>  parent_ { nullptr };

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

class ParallelForJob : public Job
{
public:
    void init(const i32 iteration_count, const i32 execute_batch_size);

    virtual void execute(const i32 index) = 0;

    void execute() final;

private:
    i32 iteration_count_ { -1 };
    i32 execute_batch_size_ { -1 };
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
