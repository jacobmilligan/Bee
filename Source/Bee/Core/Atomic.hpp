/*
 *  Atomic.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"
#include "Bee/Core/NumericTypes.hpp"

#ifdef BEE_ENABLE_RELACY

#define memory_order std_memory_order
#define atomic std_atomic
#define atomic_flag std_atomic_flag

#if BEE_OS_WINDOWS == 1

    #define NOMINMAX

#endif

#include <relacy/relacy.hpp>

#undef new
#undef delete
#undef malloc
#undef free
#undef memory_order
#undef atomic
#undef atomic_flag


namespace rl {


using memory_order = ::rl::std_memory_order;
template <typename T> using atomic = ::rl::std_atomic<T>;


} // namespace rl

namespace std { // NOLINT


using rl::memory_order;
using rl::mo_relaxed;
using rl::mo_consume;
using rl::mo_acquire;
using rl::mo_release;
using rl::mo_acq_rel;
using rl::mo_seq_cst;

using rl::atomic;
using rl::atomic_thread_fence;
using rl::atomic_signal_fence;

using rl::atomic_bool;
using rl::atomic_address;

using rl::atomic_char;
using rl::atomic_schar;
using rl::atomic_uchar;
using rl::atomic_short;
using rl::atomic_ushort;
using rl::atomic_int;
using rl::atomic_uint;
using rl::atomic_long;
using rl::atomic_ulong;
using rl::atomic_llong;
using rl::atomic_ullong;
//    using rl::atomic_char16_t;
//    using rl::atomic_char32_t;
using rl::atomic_wchar_t;

//    using rl::atomic_int_least8_t;
//    using rl::atomic_uint_least8_t;
//    using rl::atomic_int_least16_t;
//    using rl::atomic_uint_least16_t;
//    using rl::atomic_int_least32_t;
//    using rl::atomic_uint_least32_t;
//    using rl::atomic_int_least64_t;
//    using rl::atomic_uint_least64_t;
//    using rl::atomic_int_fast8_t;
//    using rl::atomic_uint_fast8_t;
//    using rl::atomic_int_fast16_t;
//    using rl::atomic_uint_fast16_t;
//    using rl::atomic_int_fast32_t;
//    using rl::atomic_uint_fast32_t;
//    using rl::atomic_int_fast64_t;
//    using rl::atomic_uint_fast64_t;
using rl::atomic_intptr_t;
using rl::atomic_uintptr_t;
using rl::atomic_size_t;
//    using rl::atomic_ssize_t;
using rl::atomic_ptrdiff_t;
//    using rl::atomic_intmax_t;
//    using rl::atomic_uintmax_t;

using rl::mutex;
using rl::recursive_mutex;
using rl::condition_variable;
using rl::condition_variable_any;

using atomic_int32_t = rl::atomic<bee::i32>;

/*
 * atomic_flag is missing from relacy
 */
struct atomic_flag {
public:
    atomic_flag() = default;

    atomic_flag(atomic_flag const&) = delete;
    atomic_flag(atomic_flag&&) = delete;
    atomic_flag& operator=(atomic_flag const&) = delete;
    atomic_flag& operator=(atomic_flag&&) = delete;

    explicit atomic_flag(bool initialValue) : val(initialValue ? 1 : 0) { }

    void clear()
    {
        clear(std::memory_order_seq_cst);
    }

    void clear(rl::memory_order mem_order, rl::debug_info_param debug_info)
    {
        val.store(0, mem_order, debug_info);
    }

    bool test_and_set()
    {
        return test_and_set(std::memory_order_seq_cst);
    }

    bool test_and_set(rl::memory_order mem_order, rl::debug_info_param debug_info)
    {
        return val.fetch_or(1, mem_order, debug_info) != 0;
    }

private:
    std::atomic<int> val;
};


} // namespace std
#else
    #include <atomic>
#endif // BEE_ENABLE_RELACY

namespace bee {

using atomic_bool   = std::atomic_bool;

using atomic_i8     = std::atomic_int8_t;
using atomic_i16    = std::atomic_int16_t;
using atomic_i32    = std::atomic_int32_t;
using atomic_i64    = std::atomic_int64_t;

using atomic_u8     = std::atomic_uint8_t;
using atomic_u16    = std::atomic_uint16_t;
using atomic_u32    = std::atomic_uint32_t;
using atomic_u64    = std::atomic_uint64_t;

using atomic_isize  = std::atomic_intptr_t;


} // namespace bee