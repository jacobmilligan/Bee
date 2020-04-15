/*
 *  Win32Concurrency.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Win32/MinWindows.h"

namespace bee {


Semaphore::Semaphore(const i32 initial_count, const i32 max_count)
    : sem_(nullptr)
{
    sem_ = CreateSemaphore(nullptr, static_cast<LONG>(initial_count), static_cast<LONG>(max_count), nullptr);
}

Semaphore::Semaphore(const i32 initial_count, const i32 max_count, const char* name)
    : sem_(nullptr)
{
    sem_ = CreateSemaphore(nullptr, static_cast<LONG>(initial_count), static_cast<LONG>(max_count), name);
}

Semaphore::~Semaphore()
{
    if (sem_ != nullptr)
    {
        ::CloseHandle(sem_);
        sem_ = nullptr;
    }
}

bool Semaphore::try_acquire()
{
    return ::WaitForSingleObject(sem_, 0L) == WAIT_OBJECT_0;
}

void Semaphore::acquire()
{
    ::WaitForSingleObject(sem_, INFINITE);
}

void Semaphore::release()
{
    ::ReleaseSemaphore(sem_, 1, nullptr);
}

void Semaphore::release(const i32 count)
{
    ::ReleaseSemaphore(sem_, static_cast<LONG>(count), nullptr);
}


Barrier::Barrier(const i32 thread_count)
    : Barrier(thread_count, -1)
{}

Barrier::Barrier(const i32 thread_count, const i32 spin_count)
{
    ::InitializeSynchronizationBarrier(&barrier_, static_cast<LONG>(thread_count), static_cast<LONG>(spin_count));
}

Barrier::~Barrier()
{
    ::DeleteSynchronizationBarrier(&barrier_);
}

void Barrier::wait()
{
    ::EnterSynchronizationBarrier(&barrier_, 0);
}


ReaderWriterMutex::ReaderWriterMutex() noexcept
{
    ::InitializeSRWLock(&mutex_);
}

void ReaderWriterMutex::lock_read()
{
    ::AcquireSRWLockShared(&mutex_);
}

bool ReaderWriterMutex::try_lock_read()
{
    return ::TryAcquireSRWLockShared(&mutex_) != FALSE;
}

void ReaderWriterMutex::unlock_read()
{
    ::ReleaseSRWLockShared(&mutex_);
}

void ReaderWriterMutex::lock_write()
{
    ::AcquireSRWLockExclusive(&mutex_);
}

bool ReaderWriterMutex::try_lock_write()
{
    return ::TryAcquireSRWLockExclusive(&mutex_) != FALSE;
}

void ReaderWriterMutex::unlock_write()
{
    ::ReleaseSRWLockExclusive(&mutex_);
}


} // namespace bee