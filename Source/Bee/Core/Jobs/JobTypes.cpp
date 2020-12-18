/*
 *  JobTypes.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

namespace bee {


JobGroup::JobGroup(Allocator* allocator) noexcept
    : parents_(allocator)
{}

JobGroup::JobGroup(JobGroup&& other) noexcept
{
    move_construct(other);
}

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

JobGroup& JobGroup::operator=(JobGroup&& other) noexcept
{
    move_construct(other);
    return *this;
}

void JobGroup::move_construct(JobGroup& other) noexcept
{
    job_wait(this);
    job_wait(&other);

    pending_count_ = other.pending_count_.load(std::memory_order_relaxed);
    dependency_count_ = other.dependency_count_.load(std::memory_order_relaxed);
    parents_ = BEE_MOVE(other.parents_);
}

void JobGroup::add_job(Job* job)
{
    job->set_group(this);
    pending_count_.fetch_add(1, std::memory_order_release);
}

void JobGroup::add_dependency(JobGroup* child_group)
{
    scoped_rw_write_lock_t lock(child_group->parents_mutex_);
    const auto index = find_index(child_group->parents_, this);
    if (index < 0)
    {
        child_group->parents_.push_back(this);
        dependency_count_.fetch_add(1, std::memory_order_release);
    }
}

i32 JobGroup::pending_count() const
{
    return pending_count_.load(std::memory_order_seq_cst);
}

i32 JobGroup::dependency_count() const
{
    return dependency_count_.load(std::memory_order_seq_cst);
}

bool JobGroup::has_pending_jobs() const
{
    return pending_count() > 0;
}

bool JobGroup::has_dependencies() const
{
    return dependency_count() > 0;
}

void JobGroup::signal(Job* job)
{
    if (job->parent() != this)
    {
        return;
    }

    const auto old_job_count = pending_count_.fetch_sub(1, std::memory_order_release);
    if (old_job_count == 0)
    {
        int new_count = -1;
        pending_count_.compare_exchange_strong(new_count, 0);
    }

    scoped_rw_write_lock_t lock(parents_mutex_);

    // Signal all the groups parents
    if (!parents_.empty())
    {
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
}

Job::Job()
    : parent_(nullptr)
{}

Job::~Job()
{
    if (parent() != nullptr)
    {
        parent()->signal(this);
    }

    parent_.store(nullptr, std::memory_order_release);
}

void Job::complete()
{
    execute();

    // Ensure all parents know about this job finishing
    if (parent() != nullptr)
    {
        parent()->signal(this);
    }

    parent_.store(nullptr, std::memory_order_release);
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


} // namespace bee