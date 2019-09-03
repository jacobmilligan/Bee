/*
 *  Allocator.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"
#include "Bee/Core/Error.hpp"
#include "Bee/Core/Noncopyable.hpp"

#if BEE_CONFIG_ENABLE_MEMORY_TRACKING == 1

#include "Bee/Core/Memory/MemoryTracker.hpp"

#endif // BEE_CONFIG_ENABLE_MEMORY_TRACKING == 1

#include <new>

namespace bee {


/*
 ****************************************************************************************
 *
 * # Allocator
 *
 * Interface for defining a memory allocator that can be used with all Skyrocket
 * containers and memory tracking systems
 *
 ****************************************************************************************
 */
class BEE_CORE_API Allocator : public Noncopyable
{
public:
    static constexpr size_t uninitialized_alloc_pattern = 0xF00DD00D;

    static constexpr size_t deallocated_memory_pattern = 0xBAADF00D;

    virtual ~Allocator() = default;

    virtual bool is_valid(const void* ptr) const = 0;

    inline virtual bool allocator_proxy_disable_tracking() const { return false; }

    virtual void* allocate(size_t size, size_t alignment) = 0;

    virtual void* allocate(const size_t size)
    {
        return allocate(size, 1);
    }

    virtual void* reallocate(void* ptr, size_t old_size, size_t new_size, size_t alignment) = 0;

    virtual void deallocate(void* ptr) = 0;
};



/*
 ****************************************************************************************
 *
 * # Allocator helper functions
 *
 * Helpers used to implement the allocation macros below. Allow for overloading the
 * allocator parameter to either a pointer or a reference
 *
 ****************************************************************************************
 */
namespace allocator_helpers {


BEE_FORCE_INLINE void* allocate_ref_or_ptr(Allocator* allocator, const size_t size, const size_t alignment)
{
    return allocator->allocate(size, alignment);
}

BEE_FORCE_INLINE void* allocate_ref_or_ptr(Allocator& allocator, const size_t size, const size_t alignment)
{
    return allocator.allocate(size, alignment);
}

BEE_FORCE_INLINE void deallocate_ref_or_ptr(Allocator* allocator, void* ptr)
{
    allocator->deallocate(ptr);
}

BEE_FORCE_INLINE void deallocate_ref_or_ptr(Allocator& allocator, void* ptr)
{
    return allocator.deallocate(ptr);
}

BEE_FORCE_INLINE void* reallocate_ref_or_ptr(Allocator* allocator, void* ptr, size_t old_size, size_t new_size, size_t alignment)
{
    return allocator->reallocate(ptr, old_size, new_size, alignment);
}

BEE_FORCE_INLINE void* reallocate_ref_or_ptr(Allocator& allocator, void* ptr, size_t old_size, size_t new_size, size_t alignment)
{
    return allocator.reallocate(ptr, old_size, new_size, alignment);
}


} // namespace allocator_helpers


template <typename ValueType>
BEE_FORCE_INLINE void destruct(ValueType* ptr)
{
    ptr->~ValueType();
}


/*
 ****************************************************************************************
 *
 * # Allocation Macros
 *
 * In general all allocations should be made through these macros as they allow for
 * tracking memory usage. The only time that Allocator::allocate, deallocate etc. should
 * be called directly is when using a backing allocator that does the heavy lifting
 * of another allocator interface.
 *
 * ## `BEE_MALLOC_ALIGNED`
 *
 * Allocates aligned memory using the allocators `allocate` function with the alignment
 * parameter
 *
 * ## `BEE_MALLOC`
 *
 * The same as `BEE_MALLOC_ALIGNED` but with an alignment of 1
 *
 * ## `BEE_REALLOC`
 *
 * Reallocates memory previously allocated with `BEE_MALLOC`, `BEE_MALLOC_ALIGNED`, or
 * `BEE_REALLOC` using allocators `reallocate` function
 *
 * ## `BEE_FREE`
 *
 * Deallocates memory previously allocated with the above macros using the allocators
 * `deallocate` function
 *
 ****************************************************************************************
 */
#if BEE_CONFIG_ENABLE_MEMORY_TRACKING == 1

    #define BEE_MALLOC_ALIGNED(allocator, size, alignment) bee::memory_tracker::allocate_tracked(allocator, size, alignment)

    #define BEE_MALLOC(allocator, size) bee::memory_tracker::allocate_tracked(allocator, size, 1)

    #define BEE_REALLOC(allocator, ptr, old_size, new_size, alignment) bee::memory_tracker::reallocate_tracked(allocator, ptr, old_size, new_size, alignment)

    #define BEE_FREE(allocator, ptr) bee::memory_tracker::deallocate_tracked(allocator, ptr)

    /**
     * Tags the allocator so that any allocations made from it aren't tracked and no external allocation events are
     * recorded for it - i.e. for a linear stack allocator that mallocs a single chunk of memory internally and
     * simply resets a cursor, this type of allocator can't leak memory by design
     */
    #define BEE_ALLOCATOR_DO_NOT_TRACK inline bool allocator_proxy_disable_tracking() const override { return true; }
#else

    #define BEE_MALLOC_ALIGNED(allocator, size, alignment) bee::allocator_helpers::allocate_ref_or_ptr(allocator, size, alignment)

    #define BEE_MALLOC(allocator, size) bee::allocator_helpers::allocate_ref_or_ptr(allocator, size, 1)

    #define BEE_REALLOC(allocator, ptr, old_size, new_size, alignment) bee::allocator_helpers::reallocate_ref_or_ptr(allocator, ptr, old_size, new_size, alignment)

    #define BEE_FREE(allocator, ptr) bee::allocator_helpers::deallocate_ref_or_ptr(allocator, ptr)

    #define BEE_ALLOCATOR_DO_NOT_TRACK

#endif // BEE_CONFIG_ENABLE_MEMORY_TRACKING == 1


/**
 * A replacement for the standard `new` operator. This should always be used in place `new` as it enables memory
 * tracking on the allocated object and allows usage of the Skyrocket allocator model
 */
#define BEE_NEW(allocator, object_type) new (BEE_MALLOC_ALIGNED(allocator, sizeof(object_type), alignof(object_type))) object_type

/**
 * Deletes an object previously allocated with `BEE_NEW` - should always be used in place of the standard c++ `delete`
 * as this enables memory tracking and usage of the Skyrocket allocator model
 */
#define BEE_DELETE(allocator, ptr) bee::destruct(ptr); BEE_FREE(allocator, ptr)


/*
 ****************************************************************************************
 *
 * # Global system allocator
 *
 * This is the systems preferred global malloc allocator
 * (i.e. on PC will use a standard MallocAllocator). Guaranteed to be thread-safe.
 *
 ****************************************************************************************
 */
BEE_CORE_API Allocator* system_allocator() noexcept;


/*
 ****************************************************************************************
 *
 * # Global temporary allocator
 *
 * A simple stack allocator whose cursor is reset at the most convenient point for the
 * application (usually at the beginning of a new frame). Allocations made with this
 * allocator are not guaranteed to last for more than a single frame but **may** remain
 * for 1-3 frames depending on the implementation. In general this should only be used
 * for allocations that will last less than the current frame to guarantee no memory
 * corruption occurs
 *
 ****************************************************************************************
 */
BEE_CORE_API Allocator* temp_allocator() noexcept;

BEE_CORE_API void reset_temp_allocator() noexcept;

/*
 **********************************************************
 *
 * Dumps an allocation report with all recorded allocation
 * event to the current log file (stdout by default)
 *
 **********************************************************
 */
BEE_CORE_API void log_allocations();


} // namespace bee
