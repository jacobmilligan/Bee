/*
 *  JobDependencyCache.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Jobs/JobDependencyCache.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"


namespace bee {


JobDependencyCache::JobDependencyCache(Allocator* allocator) noexcept
    : allocator_(sizeof(WaitHandle), alignof(WaitHandle), 1),
      wait_handles_(allocator),
      to_erase_(allocator)
{}

JobDependencyCache::~JobDependencyCache()
{
    job_wait(&all_jobs_);
}

JobDependencyCache::WaitHandle* JobDependencyCache::get_or_create_wait_handle(const u32 hash)
{
    scoped_recursive_lock_t lock(mutex_);

    auto* mapped = wait_handles_.find(hash);

    if (mapped != nullptr)
    {
        return mapped->value;
    }

    return wait_handles_.insert(hash, BEE_NEW(allocator_, WaitHandle))->value;
}

JobDependencyCache::WaitHandle* JobDependencyCache::get_wait_handle(const u32 hash)
{
    scoped_recursive_lock_t lock(mutex_);
    auto* mapped = wait_handles_.find(hash);
    return mapped != nullptr ? mapped->value : nullptr;
}

void JobDependencyCache::schedule_write(const u32 hash, Job* job, JobGroup* parent_group)
{
    auto* wait_handle = get_or_create_wait_handle(hash);

    job_wait(&wait_handle->write_deps);
    job_wait(&wait_handle->read_deps);
    all_jobs_.add_dependency(&wait_handle->write_deps);

    if (parent_group != nullptr)
    {
        parent_group->add_dependency(&wait_handle->write_deps);
    }

    job_schedule(&wait_handle->write_deps, job);
}

void JobDependencyCache::schedule_read(const u32 hash, Job* job, JobGroup* parent_group)
{
    auto* wait_handle = get_or_create_wait_handle(hash);

    job_wait(&wait_handle->write_deps);
    all_jobs_.add_dependency(&wait_handle->read_deps);

    if (parent_group != nullptr)
    {
        parent_group->add_dependency(&wait_handle->read_deps);
    }

    job_schedule(&wait_handle->read_deps, job);
}

void JobDependencyCache::wait(const u32 hash)
{
    auto* wait_handle = get_wait_handle(hash);
    if (wait_handle == nullptr)
    {
        return;
    }

    job_wait(&wait_handle->write_deps);
    job_wait(&wait_handle->read_deps);
}

void JobDependencyCache::wait_read(const u32 hash)
{
    auto* wait_handle = get_wait_handle(hash);
    if (wait_handle == nullptr)
    {
        return;
    }

    job_wait(&wait_handle->read_deps);
}

void JobDependencyCache::wait_write(const u32 hash)
{
    auto* wait_handle = get_wait_handle(hash);
    if (wait_handle == nullptr)
    {
        return;
    }

    job_wait(&wait_handle->write_deps);
}

void JobDependencyCache::wait_all()
{
    job_wait(&all_jobs_);
}

void JobDependencyCache::trim()
{
    scoped_recursive_lock_t lock(mutex_);

    to_erase_.clear();

    for (auto& group : wait_handles_)
    {
        if (!group.value->write_deps.has_pending_jobs() && !group.value->read_deps.has_pending_jobs())
        {
            group.value = nullptr;

            to_erase_.push_back(group.key);
        }
    }

    for (const auto& hash : to_erase_)
    {
        wait_handles_.erase(hash);
    }
}


} // namespace bee