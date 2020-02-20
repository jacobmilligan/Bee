/*
 *  JobTypes.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

namespace bee {


JobGroup::JobGroup(Allocator* allocator)
    : parents_(allocator)
{}

JobGroup::~JobGroup()
{
    BEE_ASSERT(!has_pending_jobs());

    scoped_rw_read_lock_t lock(parents_mutex_);

    for (JobGroup* parent : parents_)
    {
        parent->pending_count_.fetch_sub(1, std::memory_order_release);
    }

    parents_.clear();
    parents_.shrink_to_fit();
}

void JobGroup::add_job(Job* job)
{
    job->set_group(this);
    pending_count_.fetch_add(1, std::memory_order_release);
}

void JobGroup::add_dependency(JobGroup* child_group)
{
    scoped_rw_write_lock_t lock(child_group->parents_mutex_);
    child_group->parents_.push_back(this);
    dependency_count_.fetch_add(1, std::memory_order_release);
}

i32 JobGroup::pending_count()
{
    return pending_count_.load(std::memory_order_seq_cst);
}

i32 JobGroup::dependency_count()
{
    return dependency_count_.load(std::memory_order_seq_cst);
}

bool JobGroup::has_pending_jobs()
{
    return pending_count() > 0;
}

bool JobGroup::has_dependencies()
{
    return dependency_count() > 0;
}

void JobGroup::signal(Job* job)
{
    if (job->parent() != this)
    {
        return;
    }

    scoped_rw_write_lock_t lock(parents_mutex_);

    const auto old_job_count = pending_count_.fetch_sub(1, std::memory_order_release);
    if (old_job_count == 0)
    {
        int new_count = -1;
        pending_count_.compare_exchange_strong(new_count, 0);
    }

    // Signal all the groups parents
    for (JobGroup* parent : parents_)
    {
        const auto old_dep_count = parent->dependency_count_.fetch_sub(1, std::memory_order_release);
        if (old_dep_count == 0)
        {
            int new_count = -1;
            parent->dependency_count_.compare_exchange_strong(new_count, 0, std::memory_order_release);
        }
    }

    // Ensure all memory allocated by the array is free'd as soon as possible for i.e. job temp allocations that
    // need to be used quickly
    parents_.clear();
    parents_.shrink_to_fit();
}

Job::Job()
    : owning_worker_(get_local_job_worker_id()),
      parent_(nullptr)
{}

Job::Job(bee::Job&& other) noexcept
{
    move_construct(other);
}

Job::~Job()
{
    owning_worker_.store(-1);
    parent_.store(nullptr, std::memory_order_release);
}

Job& Job::operator=(bee::Job&& other) noexcept
{
    move_construct(other);
    return *this;
}

void Job::move_construct(Job& other) noexcept
{
    parent_.store(other.parent_, std::memory_order_acq_rel);
    owning_worker_.store(other.owning_worker_.load());

    other.parent_.store(nullptr, std::memory_order_acq_rel);
    other.owning_worker_.store(-1);
}

void Job::complete()
{
    execute();

    BEE_ASSERT(parent() != nullptr);
    BEE_ASSERT(owning_worker_.load() != -1);

    // Ensure all parents know about this job finishing
    parent()->signal(this);
}

void Job::set_group(JobGroup* group)
{
    if (parent() != nullptr)
    {
        parent()->signal(this);
    }

    parent_.store(group, std::memory_order_release);
}

JobGroup* Job::parent() const
{
    return parent_.load(std::memory_order_acquire);
}

i32 Job::owning_worker_id() const
{
    return owning_worker_.load(std::memory_order_acquire);
}

void ParallelForJob::init(const i32 iteration_count, const i32 execute_batch_size)
{
    BEE_ASSERT_F(iteration_count_ == -1 && execute_batch_size_ == -1, "ParallelForJob has already been initialized");

    iteration_count_ = iteration_count;
    execute_batch_size_ = execute_batch_size;
}

void ParallelForJob::execute()
{
    auto function = [&](const i32 begin, const i32 end)
    {
        for (int i = begin; i < end; ++i)
        {
            execute(i);
        }
    };

    const auto first_batch_size = math::min(iteration_count_, execute_batch_size_);

    for (int batch = first_batch_size; batch < iteration_count_; batch += execute_batch_size_)
    {
        const auto batch_end = math::min(iteration_count_, batch + execute_batch_size_);
        auto loop_job = allocate_job(function, batch, batch_end);
        job_schedule(parent(), loop_job);
    }

    for (int i = 0; i < first_batch_size; ++i)
    {
        execute(i);
    }
}


} // namespace bee