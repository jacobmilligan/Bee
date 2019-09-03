/*
 *  Thread.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Error.hpp"
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Thread.hpp"
#include "Bee/Core/String.hpp"

#include <errno.h>

namespace bee {

/*
 * Thread implementation
 */

void Thread::init(const ThreadCreateInfo& create_info, ExecuteParams* params)
{
    const auto name_len = str::length(create_info.name);
    str::copy(name_, max_name_length, create_info.name, name_len);
    if (name_len == 0)
    {
        str::copy(name_, max_name_length, "Skyrocket.Thread");
    }
    create_native_thread(params);
}

Thread::Thread(Thread&& other) noexcept
{
    move_construct(other);
}

Thread& Thread::operator=(Thread&& other) noexcept
{
    move_construct(other);
    return *this;
}

Thread::~Thread()
{
    if (native_thread_ == nullptr)
    {
        return;
    }
    join();
}

void Thread::move_construct(Thread& other) noexcept
{
    if (BEE_FAIL_F(!joinable(), "Cannot destroy a joinable thread"))
    {
        return;
    }

    str::copy(name_, max_name_length, other.name_);
    native_thread_ = other.native_thread_;

    other.native_thread_ = nullptr;
}


} // namespace bee
