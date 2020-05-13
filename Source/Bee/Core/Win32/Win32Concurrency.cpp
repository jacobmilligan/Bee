/*
 *  Win32Concurrency.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Win32/MinWindows.h"

namespace bee {


Semaphore::Semaphore(const i32 initial_count, const i32 max_count) noexcept
    : native_handle(nullptr)
{
    native_handle = CreateSemaphore(nullptr, static_cast<LONG>(initial_count), static_cast<LONG>(max_count), nullptr);
}

Semaphore::Semaphore(const i32 initial_count, const i32 max_count, const char* name) noexcept
    : native_handle(nullptr)
{
    native_handle = CreateSemaphore(nullptr, static_cast<LONG>(initial_count), static_cast<LONG>(max_count), name);
}

Semaphore::~Semaphore()
{
    if (native_handle != nullptr)
    {
        ::CloseHandle(native_handle);
        native_handle = nullptr;
    }
}

bool Semaphore::try_acquire()
{
    return ::WaitForSingleObject(native_handle, 0L) == WAIT_OBJECT_0;
}

void Semaphore::acquire()
{
    ::WaitForSingleObject(native_handle, INFINITE);
}

void Semaphore::release()
{
    ::ReleaseSemaphore(native_handle, 1, nullptr);
}

void Semaphore::release(const i32 count)
{
    ::ReleaseSemaphore(native_handle, static_cast<LONG>(count), nullptr);
}


Barrier::Barrier(const i32 thread_count) noexcept
    : Barrier(thread_count, -1)
{}

Barrier::Barrier(const i32 thread_count, const i32 spin_count) noexcept
{
    ::InitializeSynchronizationBarrier(&native_handle, static_cast<LONG>(thread_count), static_cast<LONG>(spin_count));
}

Barrier::~Barrier()
{
    ::DeleteSynchronizationBarrier(&native_handle);
}

void Barrier::wait()
{
    ::EnterSynchronizationBarrier(&native_handle, 0);
}


ReaderWriterMutex::ReaderWriterMutex() noexcept
{
    ::InitializeSRWLock(&native_handle);
}

void ReaderWriterMutex::lock_read()
{
    ::AcquireSRWLockShared(&native_handle);
}

bool ReaderWriterMutex::try_lock_read()
{
    return ::TryAcquireSRWLockShared(&native_handle) != FALSE;
}

void ReaderWriterMutex::unlock_read()
{
    ::ReleaseSRWLockShared(&native_handle);
}

void ReaderWriterMutex::lock_write()
{
    ::AcquireSRWLockExclusive(&native_handle);
}

bool ReaderWriterMutex::try_lock_write()
{
    return ::TryAcquireSRWLockExclusive(&native_handle) != FALSE;
}

void ReaderWriterMutex::unlock_write()
{
    ::ReleaseSRWLockExclusive(&native_handle);
}

Mutex::Mutex() noexcept
{
    ::InitializeCriticalSection(&native_handle);
}

Mutex::~Mutex()
{
    ::DeleteCriticalSection(&native_handle);
}

void Mutex::lock()
{
    ::EnterCriticalSection(&native_handle);
}

void Mutex::unlock()
{
    ::LeaveCriticalSection(&native_handle);
}

bool Mutex::try_lock()
{
    return ::TryEnterCriticalSection(&native_handle) != 0;
}

RecursiveMutex::RecursiveMutex() noexcept
{
    ::InitializeCriticalSection(&native_handle);
}

RecursiveMutex::~RecursiveMutex()
{
    ::DeleteCriticalSection(&native_handle);
}

void RecursiveMutex::lock()
{
    ::EnterCriticalSection(&native_handle);
}

void RecursiveMutex::unlock()
{
    ::LeaveCriticalSection(&native_handle);
}

bool RecursiveMutex::try_lock()
{
    return ::TryEnterCriticalSection(&native_handle) != 0;
}

ConditionVariable::ConditionVariable() noexcept
{
    ::InitializeConditionVariable(&native_handle);
}

void ConditionVariable::notify_one() noexcept
{
    ::WakeConditionVariable(&native_handle);
}

void ConditionVariable::notify_all() noexcept
{
    ::WakeAllConditionVariable(&native_handle);
}

void ConditionVariable::wait(ScopedLock<Mutex>& lock)
{
    ::SleepConditionVariableCS(&native_handle, &lock.mutex()->native_handle, INFINITE);
}

bool ConditionVariable::wait_for(ScopedLock<Mutex>& lock, const TimePoint& duration)
{
    return ::SleepConditionVariableCS(&native_handle, &lock.mutex()->native_handle, static_cast<DWORD>(duration.milliseconds())) != 0;
}

bool ConditionVariable::wait_until(ScopedLock<Mutex>& lock, const TimePoint& abs_time)
{
    const TimePoint now(time::now());
    const auto relative_time = abs_time - now;
    return now < abs_time ? wait_for(lock, relative_time) : false;
}



} // namespace bee