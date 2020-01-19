/*
 *  Win32Concurrency.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Concurrency.hpp"

namespace bee {


ReaderWriterMutex::ReaderWriterMutex()
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