/*
 *  JobTypes.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Jobs/JobSystem.hpp"

namespace bee {


Job::Job()
    : dependency_count_(1),
      owning_worker_(get_local_job_worker_id()),
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
    dependency_count_.store(other.dependency_count_, std::memory_order_acq_rel);
    parent_.store(other.parent_, std::memory_order_acq_rel);
    owning_worker_ = other.owning_worker_;

    other.dependency_count_.store(0, std::memory_order_acq_rel);
    other.parent_.store(nullptr, std::memory_order_acq_rel);
    other.owning_worker_ = -1;
}

void Job::complete()
{
    execute();

    // Ensure all parents know about this job finishing
    if (parent() != nullptr)
    {
        parent()->signal_completed_dependency();
    }
    signal_completed_dependency();
}

void Job::add_dependency(bee::Job* dependency)
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

i32 Job::dependency_count() const
{
    return dependency_count_.load(std::memory_order_acquire);
}

bool Job::has_dependencies() const
{
    return dependency_count() > 0;
}

Job* Job::parent() const
{
    return parent_.load(std::memory_order_acquire);
}

i32 Job::owning_worker_id() const
{
    return owning_worker_;
}

void Job::signal_completed_dependency()
{
    dependency_count_.fetch_sub(1, std::memory_order_acq_rel);
}


} // namespace bee