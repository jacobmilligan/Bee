/*
 *  JobDependencyCache.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/PoolAllocator.hpp"
#include "Bee/Core/Jobs/JobTypes.hpp"
#include "Bee/Core/Containers/HashMap.hpp"


namespace bee {


class BEE_CORE_API JobDependencyCache
{
public:
    explicit JobDependencyCache(Allocator* allocator = system_allocator()) noexcept;

    ~JobDependencyCache();

    void schedule_write(const u32 hash, Job* job, JobGroup* parent_group = nullptr);

    void schedule_read(const u32 hash, Job* job, JobGroup* parent_group = nullptr);

    void wait(const u32 hash);

    void wait_read(const u32 hash);

    void wait_write(const u32 hash);

    void wait_all();

    void trim();

    template <typename T>
    inline void schedule_write(const T& value, Job* job, JobGroup* parent_group = nullptr)
    {
        schedule_write(get_hash(value), job, parent_group);
    }

    template <typename T>
    inline void schedule_read(const T& value, Job* job, JobGroup* parent_group = nullptr)
    {
        return schedule_read(get_hash(value), job, parent_group);
    }

private:
    struct WaitHandle
    {
        JobGroup write_deps;
        JobGroup read_deps;
    };

    RecursiveMutex                      mutex_;
    PoolAllocator                       allocator_;
    JobGroup                            all_jobs_;
    DynamicHashMap<u32, WaitHandle*>    wait_handles_;
    DynamicArray<u32>                   to_erase_;

    WaitHandle* get_or_create_wait_handle(const u32 hash);

    WaitHandle* get_wait_handle(const u32 hash);
};


} // namespace bee