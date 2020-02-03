/*
 *  Concurrency.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Thread.hpp"

#if BEE_OS_WINDOWS == 1
    #include "Bee/Core/Win32/Win32Concurrency.hpp"
#else
    #error Platform not supported
#endif // BEE_OS_WINDOWS == 1

#include <atomic>


namespace bee {
namespace concurrency {


BEE_CORE_API u32 physical_core_count();

BEE_CORE_API u32 logical_core_count();


} // namespace concurrency


class BEE_CORE_API Semaphore final
{
public:
    Semaphore(const i32 initial_count, const i32 max_count);

    Semaphore(const i32 initial_count, const i32 max_count, const char* name);

    ~Semaphore();

    bool try_acquire();

    void acquire();

    void release();

    void release(const i32 count);

private:
    native_semaphore_t sem_;
};


class BEE_CORE_API Barrier
{
public:
    explicit Barrier(const i32 thread_count);

    Barrier(const i32 thread_count, const i32 spin_count);

    ~Barrier();

    void wait();

private:
    native_barrier_t barrier_;
};


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

class BEE_CORE_API ReaderWriterMutex
{
public:
    ReaderWriterMutex();

    void lock_read();

    bool try_lock_read();

    void unlock_read();

    void lock_write();

    bool try_lock_write();

    void unlock_write();
private:
    native_rw_mutex_t mutex_;
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

template <typename MutexType>
class ScopedReaderLock
{
public:
    explicit ScopedReaderLock(MutexType& mutex)
        : mutex_(mutex)
    {
        mutex.lock_read();
    }

    ScopedReaderLock(const ScopedReaderLock&) = delete;

    ~ScopedReaderLock()
    {
        mutex_.unlock_read();
    }

    ScopedReaderLock& operator=(const ScopedReaderLock&) = delete;
private:
    MutexType& mutex_;
};

template <typename MutexType>
class ScopedWriterLock
{
public:
    explicit ScopedWriterLock(MutexType& mutex)
        : mutex_(mutex)
    {
        mutex.lock_write();
    }

    ScopedWriterLock(const ScopedWriterLock&) = delete;

    ~ScopedWriterLock()
    {
        mutex_.unlock_write();
    }

    ScopedWriterLock& operator=(const ScopedWriterLock&) = delete;
private:
    MutexType& mutex_;
};

using scoped_spinlock_t = ScopedLock<SpinLock>;
using scoped_recursive_spinlock_t = ScopedLock<RecursiveSpinLock>;
using scoped_rw_read_lock_t = ScopedReaderLock<ReaderWriterMutex>;
using scoped_rw_write_lock_t = ScopedWriterLock<ReaderWriterMutex>;


} // namespace bee