/*
 *  PosixThread.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <pthread.h>

namespace bee {


using native_rw_mutex_t = pthread_rwlock_t;
using native_mutex_t = pthread_mutex_t;


} // namespace bee
