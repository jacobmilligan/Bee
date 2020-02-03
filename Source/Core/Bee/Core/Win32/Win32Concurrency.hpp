/*
 *  Win32Concurrency.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Win32/MinWindowsArch.h"
#include <synchapi.h>

namespace bee {


using native_rw_mutex_t = SRWLOCK;
using native_mutex_t = SRWLOCK;
using native_semaphore_t = HANDLE;
using native_barrier_t = SYNCHRONIZATION_BARRIER;


} // namespace bee