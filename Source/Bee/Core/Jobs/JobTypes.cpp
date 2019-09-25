/*
 *  JobTypes.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Jobs/JobSystem.hpp"

namespace bee {


void JobGroup::add_job(Job* job)
{
    job->set_group(this);
    pending_count_.fetch_add(1, std::memory_order_release);
}

i32 JobGroup::pending_count()
{
    return pending_count_.load(std::memory_order_acquire);
}

bool JobGroup::has_pending_jobs()
{
    return pending_count() > 0;
}

void JobGroup::signal(Job* job)
{
    if (job->parent() != this)
    {
        return;
    }

    const auto old_count = pending_count_.fetch_sub(1, std::memory_order_release);
    if (old_count == 0)
    {
        pending_count_.store(0, std::memory_order_release);
    }
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
    owning_worker_ = -1;
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
    owning_worker_ = other.owning_worker_;

    other.parent_.store(nullptr, std::memory_order_acq_rel);
    other.owning_worker_ = -1;
}

void Job::complete()
{
    BEE_ASSERT(parent() != nullptr);

    execute();

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
    return owning_worker_;
}

} // namespace bee