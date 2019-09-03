/*
 *  Concurrency.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Thread.hpp"

#include <atomic>

namespace bee {
namespace concurrency {


BEE_CORE_API u32 physical_core_count();

BEE_CORE_API u32 logical_core_count();


} // namespace concurrency


class BEE_CORE_API SpinLock
{
public:
    void lock();

    void unlock();
private:
    std::atomic_flag lock_ { false };
};

class BEE_CORE_API RecursiveSpinLock
{
public:
    RecursiveSpinLock();

    ~RecursiveSpinLock();

    void lock();

    void unlock();
private:
    SpinLock                        lock_;
    std::atomic<bee::Thread::id_t>  owner_ { 0 };
    std::atomic_int32_t             lock_count_ { 0 };

    void unlock_and_reset();
};


template <typename MutexType>
class ScopedLock
{
public:
    explicit ScopedLock(MutexType& mutex)
        : mutex_(mutex)
    {
        mutex.lock();
    }

    ScopedLock(const ScopedLock&) = delete;

    ~ScopedLock()
    {
        mutex_.unlock();
    }

    ScopedLock& operator=(const ScopedLock&) = delete;
private:
    MutexType& mutex_;
};

using scoped_spinlock_t = ScopedLock<SpinLock>;
using scoped_recursive_spinlock_t = ScopedLock<RecursiveSpinLock>;


} // namespace bee