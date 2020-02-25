/*
 *  JobSystem.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Jobs/JobTypes.hpp"
#include "Bee/Core/Atomic.hpp"


namespace bee {


#ifndef BEE_WORKER_MAX_COMPLETED_JOBS
    #define BEE_WORKER_MAX_COMPLETED_JOBS 4096
#endif // BEE_WORKER_MAX_COMPLETED_JOBS

using job_handle_t = uintptr_t;

struct JobSystemInitInfo
{
    static constexpr i32 auto_worker_count = -1;

    i32     num_workers { auto_worker_count };
    i32     max_job_size { 512 };
    i32     max_jobs_per_worker_per_chunk { 1024 }; // max number of pooled jobs to create in a single thread-local allocation chunk
    size_t  per_worker_temp_allocator_capacity { 1024 * 16 }; // capacity of the per-worker thread-local temp allocator used for jobs
};

BEE_CORE_API bool job_system_init(const JobSystemInitInfo& info);

BEE_CORE_API void job_system_shutdown();

BEE_CORE_API void job_schedule(JobGroup* group, Job* job);

BEE_CORE_API void job_schedule_group(JobGroup* group, Job** dependencies, i32 dependency_count);

BEE_CORE_API bool job_wait(JobGroup* group);

BEE_CORE_API Job* get_local_executing_job();

BEE_CORE_API i32 get_local_job_worker_id();

BEE_CORE_API i32 get_job_worker_count();

BEE_CORE_API Job* allocate_job();

BEE_FORCE_INLINE AtomicNode* cast_job_to_node(Job* job)
{
    return reinterpret_cast<AtomicNode*>(reinterpret_cast<u8*>(job) - sizeof(AtomicNode));
}

//template <typename JobType, typename... ConstructorArgs>
//inline JobType* allocate_job(ConstructorArgs&&... args)
//{
//    auto job = BEE_NEW(local_job_allocator(), JobType)(std::forward<ConstructorArgs>(args)...);
//    BEE_ASSERT(job->parent() == nullptr);
//    return job;
//}
//
//template <typename JobType, typename... ConstructorArgs>
//inline JobType* allocate_parallel_for(const i32 iteration_count, const i32 execute_batch_size, ConstructorArgs&&... args)
//{
//    static_assert(std::is_base_of<ParallelForJob, JobType>, "JobType must derive from ParallelForJob");
//
//    auto job = BEE_NEW(local_job_allocator(), JobType)(std::forward<ConstructorArgs>(args)...);
//
//    BEE_ASSERT(job->parent() == nullptr);
//
//    // use init rather than constructor to setup parallel for parameters to avoid user having to
//    // re-implement constructor
//    job->init(iteration_count, execute_batch_size);
//
//    return job;
//}
//
//template <typename FunctionType, typename... Args>
//inline Job* allocate_job(FunctionType&& function, Args&&... args)
//{
//    using job_t = CopyArgsJob<decltype(std::bind(function, args...))>;
//    auto job = BEE_NEW(local_job_allocator(), job_t)(std::bind(function, args...));
//    BEE_ASSERT(job->parent() == nullptr);
//    return job;
//}

template <typename FunctionType, typename... Args>
Job* create_job(FunctionType&& fn, Args&&... args)
{
    auto job = allocate_job();
    auto callable = [=]() { fn(args...); };
    new (job) CallableJob<decltype(callable)>(callable);
    return job;
}

template <typename FunctionType>
Job* create_job(FunctionType&& fn)
{
    auto job = allocate_job();
    auto callable = [=]() { fn(); };
    new (job) CallableJob<decltype(callable)>(callable);
    return job;
}


template <typename FunctionType>
inline void parallel_for(JobGroup* group, const i32 iteration_count, const i32 execute_batch_size, FunctionType&& function)
{
    const auto first_batch_size = math::min(iteration_count, execute_batch_size);

    auto first_job = create_job([&, end=first_batch_size]()
    {
        for (int i = 0; i < end; ++i)
        {
            function(i);
        }
    });

    for (int batch = first_batch_size; batch < iteration_count; batch += execute_batch_size)
    {
        auto batch_job = create_job([&, begin=batch, end=math::min(iteration_count, batch + execute_batch_size)]()
        {
            for (int i = begin; i < end; ++i)
            {
                function(i);
            }
        });

        job_schedule(group, batch_job);
    }

    job_schedule(group, first_job);
}


} // namespace bee
