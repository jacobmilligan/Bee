/*
 *  JobTypes.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Atomic.hpp"


namespace bee {


class Allocator;
struct Worker;
class Job;

class BEE_CORE_API JobGroup
{
public:
    explicit JobGroup(Allocator* allocator = system_allocator()) noexcept;

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
    Job();

    virtual ~Job();

    void complete();

    JobGroup* parent() const;

    void set_group(JobGroup* group);

protected:
#ifdef BEE_ENABLE_RELACY
    static constexpr size_t alignment = 128;
#else
    static constexpr size_t alignment = 64;
#endif // BEE_ENABLE_RELACY

    static constexpr size_t data_size_ = alignment - sizeof(std::atomic<JobGroup*>);

    std::atomic<JobGroup*>  parent_ { nullptr };
    u8                      data_[data_size_];

    virtual void execute() = 0;
};


template <typename FunctionType>
struct CallableJob final : public Job
{
    static_assert(sizeof(FunctionType) <= data_size_, "CallableJob: the jobs arguments are too big to fit in its storage");

    explicit CallableJob(const FunctionType& function)
    {
        new (data_) FunctionType(function);
    }

    ~CallableJob() override
    {
        auto function = reinterpret_cast<FunctionType*>(data_);
        destruct(function);
    }

    void execute() override
    {
        auto function = reinterpret_cast<FunctionType*>(data_);
        (*function)();
    }
};


} // namespace bee
