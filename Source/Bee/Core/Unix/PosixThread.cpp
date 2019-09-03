//
//  Thread.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 18/10/18
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#include "Bee/Core/Error.hpp"
#include "Skyrocket/Platform/Thread.hpp"


namespace bee {


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

    auto data = static_cast<Thread::ThreadData*>(thread_data);
    BEE_ASSERT_F(data->function != nullptr, "Invalid thread function given");

#if BEE_OS_MACOS == 1
    const auto setname_err = pthread_setname_np(data->name.c_str());
    BEE_ASSERT_F(setname_err == 0, "Failed to set thread name: \"%s\": pthread error: %s",
               data->name, strerror(setname_err));
#endif // BEE_OS_MACOS == 1

    data->initialized = true;
    data->function(data->user_data);
    return nullptr;
}

void Thread::create_native_thread()
{
    auto pthread_err = pthread_create(&thread_data_.native_thread, nullptr, Thread::execute_cb, &thread_data_);

    while (!thread_data_.initialized) {}

    BEE_ASSERT_F(pthread_err == 0, "Failed to create thread \"%s\": pthread error: %s",
               thread_data_.name, strerror(pthread_err));

#if BEE_OS_LINUX == 1
    const auto setname_err = pthread_setname_np(native_thread_->pthread, name_.c_str());
    BEE_ASSERT_F(setname_err == 0, "Failed to set thread name: pthread error: %s",
               strerror(setname_err));
#endif // BEE_OS_LINUX == 1
}

void Thread::join()
{
    if (thread_data_.function == nullptr || thread_data_.native_thread == nullptr) {
        return;
    }

    const auto join_err = pthread_join(thread_data_.native_thread, nullptr);
    BEE_ASSERT_F(
        join_err == 0,
        "Failed to join thread \"%s\": pthread error: %s",
        thread_data_.name,
        strerror(join_err)
    );

    thread_data_.native_thread = nullptr;
    thread_data_.function = nullptr;
}

void Thread::detach()
{
    const auto detach_err = pthread_detach(thread_data_.native_thread);
    BEE_ASSERT_F(
        detach_err == 0,
        "Failed to detach thread \"%s\": pthread error: %s",
        thread_data_.name,
        strerror(detach_err)
    );
}

u64 Thread::id() const
{
    return get_pthread_id(thread_data_.native_thread);
}


} // namespace bee
