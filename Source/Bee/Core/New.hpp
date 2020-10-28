/*
 *  New.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#if _MSC_VER
    // don't override these operators if the msvc <new> has been included first
    #ifndef __PLACEMENT_NEW_INLINE
        #define __PLACEMENT_NEW_INLINE
    #else
        #define BEE_PLACEMENT_NEW_INLINE
    #endif // __PLACEMENT_NEW_INLINE
    #ifndef __PLACEMENT_VEC_NEW_INLINE
        #define __PLACEMENT_VEC_NEW_INLINE
    #else
        #define BEE_PLACEMENT_ARRAY_NEW_INLINE
    #endif // __PLACEMENT_NEW_INLINE
#endif // _MSC_VER

/*
 * Placement new operators
 */
#ifndef BEE_PLACEMENT_NEW_INLINE
#define BEE_PLACEMENT_NEW_INLINE
inline void* operator new(const size_t size, void* ptr) noexcept
{
    (void)size;
    return ptr;
}

inline void operator delete(void*, void*) noexcept
{

}
#endif // BEE_PLACEMENT_NEW_INLINE

#ifndef BEE_PLACEMENT_ARRAY_NEW_INLINE
#define BEE_PLACEMENT_ARRAY_NEW_INLINE
inline void* operator new[](size_t size, void* ptr) noexcept
{
    (void)size;
    return ptr;
}

inline void operator delete[](void*, void*) noexcept
{

}
#endif // BEE_PLACEMENT_ARRAY_NEW_INLINE