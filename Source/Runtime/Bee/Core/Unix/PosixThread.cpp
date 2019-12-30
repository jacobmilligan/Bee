/*
 *  Thread.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Error.hpp"
#include "Bee/Core/Thread.hpp"
#include "Bee/Core/Unix/PosixThread.hpp"
#include "Bee/Core/Concurrency.hpp"


namespace bee {


#define BEE_PTHREAD_NAME_LENGTH 15

std::string get_pthread_name(const pthread_t pthread)
{
    char result[BEE_PTHREAD_NAME_LENGTH];

#if BEE_OS_MACOS == 1
    pthread_getname_np(pthread, result, BEE_PTHREAD_NAME_LENGTH);
#else
    #error pthread name not implemented on linux
#endif // BEE_OS_MACOS == 1

    return std::string(result);
}

u64 get_pthread_id(const pthread_t pthread)
{
    u64 result = 0;

#if BEE_OS_MACOS == 1
    const auto pthread_id_err = pthread_threadid_np(pthread, &result);
    BEE_ASSERT_F(pthread_id_err == 0, "Failed to get id for thread \"%s\": pthread error: %s",
               get_pthread_name(pthread).c_str(), strerror(pthread_id_err));
#else
    #error id not implemented on linux
#endif // BEE_OS_MACOS == 1

    return result;
}

u64 this_thread_id()
{
    return get_pthread_id(pthread_self());
}


Thread::execute_cb_return_t Thread::execute_cb(void* thread_data)
{
    BEE_ASSERT_F(thread_data != nullptr, "Invalid data given to thread execute callback");

    auto data = static_cast<Thread::ExecuteParams*>(thread_data);
    BEE_ASSERT_F(data->function != nullptr, "Invalid thread function given");

#if BEE_OS_MACOS == 1
    const auto setname_err = pthread_setname_np(data->name);
    BEE_ASSERT_F(
        setname_err == 0, "Failed to set thread name: \"%s\": pthread error: %s",
        data->name, strerror(setname_err)
    );
#endif // BEE_OS_MACOS == 1

    data->invoker(data->function, data->arg);
    data->destructor(data->function, data->arg);
    return nullptr;
}

void Thread::create_native_thread(ExecuteParams* params) noexcept
{
    auto pthread_err = pthread_create(&native_thread_, nullptr, Thread::execute_cb, params);

    BEE_ASSERT_F(
        pthread_err == 0,
        "Failed to create thread \"%s\": pthread error: %s",
        params->name, strerror(pthread_err)
    );

#if BEE_OS_LINUX == 1
    const auto setname_err = pthread_setname_np(native_thread_->pthread, name_.c_str());
    BEE_ASSERT_F(setname_err == 0, "Failed to set thread name: pthread error: %s",
               strerror(setname_err));
#endif // BEE_OS_LINUX == 1
}

void Thread::join()
{
    BEE_ASSERT(native_thread_ != nullptr);

    const auto join_err = pthread_join(native_thread_, nullptr);

    BEE_ASSERT_F(
        join_err == 0,
        "Failed to join thread \"%s\": pthread error: %s",
        name_,
        strerror(join_err)
    );
}

void Thread::detach()
{
    const auto detach_err = pthread_detach(native_thread_);

    BEE_ASSERT_F(
        detach_err == 0,
        "Failed to detach thread \"%s\": pthread error: %s",
        name_,
        strerror(detach_err)
    );
}

void Thread::set_affinity(const int32_t cpu)
{
    BEE_ASSERT_F(native_thread_ != nullptr, "Thread: cannot set affinity for invalid thread");
    // TODO(Jacob): implement
}

void Thread::set_priority(const ThreadPriority priority)
{
    // TODO(Jacob): implement
//    const auto success = SetThreadPriority(native_thread_, translate_thread_priority(priority));
//    BEE_ASSERT_F(success != 0, "Failed to set thread priority: %s", win32_get_last_error_string());
}

u64 Thread::id() const
{
    return get_pthread_id(native_thread_);
}


ReaderWriterMutex::ReaderWriterMutex()
    : mutex_(pthread_rwlock_t{})
{
    pthread_rwlock_init(&mutex_, nullptr);
}

void ReaderWriterMutex::lock_read()
{
    pthread_rwlock_rdlock(&mutex_);
}

bool ReaderWriterMutex::try_lock_read()
{
    return pthread_rwlock_tryrdlock(&mutex_) == 0;
}

void ReaderWriterMutex::unlock_read()
{
    pthread_rwlock_unlock(&mutex_);
}

void ReaderWriterMutex::lock_write()
{
    pthread_rwlock_wrlock(&mutex_);
}

bool ReaderWriterMutex::try_lock_write()
{
    return pthread_rwlock_trywrlock(&mutex_) == 0;
}

void ReaderWriterMutex::unlock_write()
{
    pthread_rwlock_unlock(&mutex_);
}


} // namespace bee
