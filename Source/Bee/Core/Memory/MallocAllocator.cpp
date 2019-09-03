//
//  MacMalloc.cpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 26/11/18
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#include "Bee/Core/Memory/MallocAllocator.hpp"
#include "Bee/Core/Math/Math.hpp"

#if BEE_OS_MACOS == 1
    #include <malloc/malloc.h>
#elif BEE_OS_WINDOWS == 1
    #include <malloc.h>
#else
    #error Not implemented on this platform
#endif // BEE_OS_*


namespace bee {

size_t MallocAllocator::allocation_size(const void* ptr) const
{
#if BEE_OS_MACOS == 1

    return malloc_size(const_cast<void*>(ptr));

#elif BEE_OS_WINDOWS == 1

    return _aligned_msize(const_cast<void*>(ptr), 1, 0);

#else

    #error Not implemented for the current platform

#endif // BEE_OS_*
}

void* MallocAllocator::allocate(const size_t size)
{
#if BEE_OS_WINDOWS == 1

    return allocate(size, 1);

#else

    // on unix systems memalign must be given a power of 2 alignment AT LEAST as large as sizeof(void*)
    // otherwise it returns invalid argument.
    // see: http://man7.org/linux/man-pages/man3/posix_memalign.3.html
    return allocate(size, sizeof(void*));

#endif // BEE_OS_WINDOWS == 1
}

void* MallocAllocator::allocate(const size_t size, const size_t alignment)
{
    void* allocation = nullptr;

#if BEE_OS_UNIX == 1

    const auto adjusted_alignment = math::max(sizeof(void*), alignment);
    const auto result = posix_memalign(&allocation, adjusted_alignment, size);
    if (!BEE_CHECK(result == 0)) {
        return nullptr;
    }

#elif BEE_OS_WINDOWS == 1

    allocation = _aligned_malloc(size, alignment);

#endif // BEE_OS_UNIX

    return allocation;
}

void* MallocAllocator::reallocate(void* ptr, const size_t old_size, const size_t new_size, const size_t alignment)
{
    void* new_allocation = nullptr;

#if BEE_OS_UNIX == 1
    auto original_allocation_size = ptr != nullptr ? old_size : 0;
    BEE_ASSERT(ptr == nullptr || original_allocation_size == data_size(ptr));

    const auto adjusted_alignment = math::max(sizeof(void*), alignment);
    const auto result = posix_memalign(&new_allocation, adjusted_alignment, new_size);
    if (!BEE_CHECK(result == 0)) {
        return ptr;
    }

    if (ptr != nullptr) {
        const auto memcpy_len = math::min(new_size, original_allocation_size);
        memcpy(new_allocation, ptr, memcpy_len);
    }
    free(ptr);

#elif BEE_OS_WINDOWS == 1
    // TODO(Jacob): add asserts to check old size is valid - will need to do some magic here because *cough*windows*cough*
    new_allocation = _aligned_realloc(ptr, new_size, alignment);
    if (!BEE_CHECK(new_allocation != nullptr)) {
        return ptr;
    }

#endif // BEE_OS_UNIX

    return new_allocation;
}

void MallocAllocator::deallocate(void* ptr)
{
    BEE_ASSERT(is_valid(ptr));

#if BEE_OS_UNIX == 1

    free(ptr);

#elif BEE_OS_WINDOWS == 1

    _aligned_free(ptr);

#endif // BEE_OS_*
}


} // namespace bee
