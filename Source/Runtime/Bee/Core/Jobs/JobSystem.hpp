/*
 *  JobSystem.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Jobs/JobTypes.hpp"
#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Math/Math.hpp"

#include <atomic>


namespace bee {


#ifndef BEE_WORKER_MAX_COMPLETED_JOBS
    #define BEE_WORKER_MAX_COMPLETED_JOBS 4096
#endif // BEE_WORKER_MAX_COMPLETED_JOBS


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

BEE_CORE_API Allocator* local_job_allocator();

BEE_CORE_API Allocator* job_temp_allocator();

BEE_CORE_API Job* get_local_executing_job();

BEE_CORE_API i32 get_local_job_worker_id();

BEE_CORE_API i32 get_job_worker_count();

BEE_CORE_API size_t get_local_job_allocator_size();


template <typename JobType, typename... ConstructorArgs>
inline Job* allocate_job(ConstructorArgs&&... args)
{
    auto job = BEE_NEW(local_job_allocator(), JobType)(std::forward<ConstructorArgs>(args)...);
    BEE_ASSERT(job->parent() == nullptr);
    return job;
}

template <typename FunctionType, typename... Args>
inline Job* allocate_job(FunctionType&& function, Args&&... args)
{
    using job_t = FunctionJob<decltype(std::bind(function, args...))>;
    auto job = BEE_NEW(local_job_allocator(), job_t)(std::bind(function, args...));
    BEE_ASSERT(job->parent() == nullptr);
    return job;
}

template <typename FunctionType>
inline void __parallel_for_single_batch(const i32 range_begin, const i32 range_end, const FunctionType& function)
{
    for (int i = range_begin; i < range_end; ++i)
    {
        function(i);
    }
}


template <typename FunctionType>
inline void parallel_for(JobGroup* group, const i32 iteration_count, const i32 execute_batch_size, const FunctionType& function)
{
    const auto first_batch_size = math::min(iteration_count, execute_batch_size);
    auto job = allocate_job(&__parallel_for_single_batch<FunctionType>, 0, first_batch_size, function);

    for (int batch = first_batch_size; batch < iteration_count; batch += execute_batch_size)
    {
        const auto batch_end = math::min(iteration_count, batch + execute_batch_size);
        auto loop_job = allocate_job(&__parallel_for_single_batch<FunctionType>, batch, batch_end, function);
        job_schedule(group, loop_job);
    }

    job_schedule(group, job);
}


} // namespace bee
