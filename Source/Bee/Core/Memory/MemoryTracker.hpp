/*
 *  MemoryTracker.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Debug.hpp"


namespace bee {


class Allocator;


namespace memory_tracker {


enum class TrackingMode
{
    enabled,
    disabled
};


/*
 **************************************************************************************************
 *
 * # Allocator proxy
 *
 * Thread-safe API for recording allocation events and tracking `Allocator` interface
 * function calls.
 *
 **************************************************************************************************
 */

/**
 * Allows for enabling/disabling allocation tracking
 */
BEE_CORE_API void set_tracking_mode(const TrackingMode mode);

/**
 * Records a manual allocation event made from outside the Bee memory environment (i.e. from a call to `malloc`)
 */
BEE_CORE_API void record_manual_allocation(void* address, const size_t size, const size_t alignment, const i32 skipped_stack_frames);

/**
 * Erases a previously recorded manual allocation event. IMPORTANT - memory recorded by `Allocator` interfaces should
 * **never** go through this function otherwise the pointer will be recorded as a double-free when the allocator
 * deallocates it
 */
BEE_CORE_API void erase_manual_allocation(void* address);

BEE_CORE_API void* allocate_tracked(Allocator* allocator, const size_t size, const size_t alignment);

BEE_CORE_API void* reallocate_tracked(Allocator* allocator, void* ptr, const size_t old_size, const size_t new_size, const size_t alignment);

BEE_CORE_API void deallocate_tracked(Allocator* allocator, void* ptr);

inline void* allocate_tracked(Allocator& allocator, const size_t size, const size_t alignment)
{
    return allocate_tracked(&allocator, size, alignment);
}

inline void* reallocate_tracked(Allocator& allocator, void* ptr, const size_t old_size, const size_t new_size, const size_t alignment)
{
    return reallocate_tracked(&allocator, ptr, old_size, new_size, alignment);
}

inline void deallocate_tracked(Allocator& allocator, void* ptr)
{
    deallocate_tracked(&allocator, ptr);
}

struct AllocationEvent
{
    void*       address { nullptr };
    size_t      size { 0 };
    size_t      alignment { 0 };
    StackTrace  stack_trace;
};

BEE_CORE_API i32 get_tracked_allocations(AllocationEvent* dst_buffer, const i32 dst_buffer_count);

BEE_CORE_API void log_tracked_allocations(const LogVerbosity verbosity);


} // namespace memory_tracker
} // namespace bee