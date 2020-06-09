/*
 *  Concurrency.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Thread.hpp"
#include "Bee/Core/Atomic.hpp"
#include "Bee/Core/Time.hpp"

#if BEE_OS_WINDOWS == 1
    #include "Bee/Core/Win32/Win32Concurrency.hpp"
#else
    #error Platform not supported
#endif // BEE_OS_WINDOWS == 1


namespace bee {
namespace concurrency {


BEE_CORE_API u32 physical_core_count();

BEE_CORE_API u32 logical_core_count();


} // namespace concurrency


struct BEE_CORE_API Semaphore
{
    native_semaphore_t native_handle;

    Semaphore(const i32 initial_count, const i32 max_count) noexcept;

    Semaphore(const i32 initial_count, const i32 max_count, const char* name) noexcept;

    ~Semaphore();

    bool try_acquire();

    void acquire();

    void release();

    void release(const i32 count);
};


struct BEE_CORE_API Barrier
{
    native_barrier_t native_handle;

    explicit Barrier(const i32 thread_count) noexcept;

    Barrier(const i32 thread_count, const i32 spin_count) noexcept;

    ~Barrier();

    void wait();
};


struct BEE_CORE_API SpinLock
{
public:
    void lock();

    void unlock();
private:
    std::atomic_flag lock_ { false };
};

class BEE_CORE_API RecursiveSpinLock
{
public:
    RecursiveSpinLock() noexcept;

    ~RecursiveSpinLock();

    void lock();

    void unlock();
private:
    SpinLock                    lock_;
    std::atomic<thread_id_t>    owner_ { 0 };
    std::atomic_int32_t         lock_count_ { 0 };

    void unlock_and_reset();
};

struct BEE_CORE_API ReaderWriterMutex
{
    native_rw_mutex_t native_handle;

    ReaderWriterMutex() noexcept;

    void lock_read();

    bool try_lock_read();

    void unlock_read();

    void lock_write();

    bool try_lock_write();

    void unlock_write();
};

struct BEE_CORE_API Mutex
{
    native_mutex_t native_handle;

    Mutex() noexcept;

    ~Mutex();

    void lock();

    void unlock();

    bool try_lock();
};

struct BEE_CORE_API RecursiveMutex
{
    native_recursive_mutex_t native_handle;

    RecursiveMutex() noexcept;

    ~RecursiveMutex();

    void lock();

    void unlock();

    bool try_lock();
};


template <typename MutexType>
class ScopedLock
{
public:
    explicit ScopedLock(MutexType& mutex)
        : mutex_(mutex)
    {
        mutex.lock();
    }

    ScopedLock(const ScopedLock&) = delete;

    ~ScopedLock()
    {
        mutex_.unlock();
    }

    ScopedLock& operator=(const ScopedLock&) = delete;

    inline MutexType* mutex() const noexcept
    {
        return &mutex_;
    }

private:
    MutexType& mutex_;
};

template <typename MutexType>
class ScopedReaderLock
{
public:
    explicit ScopedReaderLock(MutexType& mutex)
        : mutex_(mutex)
    {
        mutex.lock_read();
    }

    ScopedReaderLock(const ScopedReaderLock&) = delete;

    ~ScopedReaderLock()
    {
        mutex_.unlock_read();
    }

    ScopedReaderLock& operator=(const ScopedReaderLock&) = delete;

    inline MutexType* mutex() const noexcept
    {
        return &mutex_;
    }

private:
    MutexType& mutex_;
};

template <typename MutexType>
class ScopedWriterLock
{
public:
    explicit ScopedWriterLock(MutexType& mutex)
        : mutex_(mutex)
    {
        mutex.lock_write();
    }

    ScopedWriterLock(const ScopedWriterLock&) = delete;

    ~ScopedWriterLock()
    {
        mutex_.unlock_write();
    }

    ScopedWriterLock& operator=(const ScopedWriterLock&) = delete;

    inline MutexType* mutex() const noexcept
    {
        return &mutex_;
    }

private:
    MutexType& mutex_;
};

using scoped_spinlock_t = ScopedLock<SpinLock>;
using scoped_lock_t = ScopedLock<Mutex>;
using scoped_recursive_lock_t = ScopedLock<RecursiveMutex>;
using scoped_recursive_spinlock_t = ScopedLock<RecursiveSpinLock>;
using scoped_rw_read_lock_t = ScopedReaderLock<ReaderWriterMutex>;
using scoped_rw_write_lock_t = ScopedWriterLock<ReaderWriterMutex>;


struct BEE_CORE_API ConditionVariable
{
    native_condition_variable_t native_handle{};

    ConditionVariable() noexcept;

    ~ConditionVariable() noexcept = default;

    void notify_one() noexcept;

    void notify_all() noexcept;

    void wait(ScopedLock<Mutex>& lock);

    template <typename Pred>
    void wait(ScopedLock<Mutex>& lock, Pred predicate)
    {
        while (!predicate())
        {
            wait(lock);
        }
    }

    bool wait_for(ScopedLock<Mutex>& lock, const TimePoint& duration);

    template <typename Pred>
    bool wait_for(ScopedLock<Mutex>& lock, const TimePoint& duration, Pred predicate)
    {
        while (!predicate())
        {
            if (!wait_for(lock, duration))
            {
                return predicate();
            }
        }

        return predicate();
    }

    bool wait_until(ScopedLock<Mutex>& lock, const TimePoint& abs_time);

    template <typename Pred>
    bool wait_until(ScopedLock<Mutex>& lock, const TimePoint& abs_time, Pred predicate)
    {
        const TimePoint now(time::now());
        const auto relative_time = abs_time - now;
        return now < abs_time ? wait_for(lock, relative_time, predicate) : false;
    }
};


/*
 ****************************************
 *
 * Lock-free containers and algorithms
 *
 ****************************************
 */
struct AtomicNode
{
    std::atomic<u64>    next { 0 };
    uintptr_t           version { 0 };
    void*               data[2] { nullptr };

    AtomicNode() = default;

    AtomicNode(AtomicNode&& other) noexcept
    {
        next.store(other.next.load(std::memory_order_relaxed), std::memory_order_relaxed);
        version = other.version;
        data[0] = other.data[0];
        data[1] = other.data[1];

        other.next.store(0, std::memory_order_relaxed);
        other.version = 0;
        data[0] = nullptr;
        data[1] = nullptr;
    }

    AtomicNode& operator=(AtomicNode&& other) noexcept
    {
        next.store(other.next.load(std::memory_order_relaxed), std::memory_order_relaxed);
        version = other.version;
        data[0] = other.data[0];
        data[1] = other.data[1];

        other.next.store(0, std::memory_order_relaxed);
        other.version = 0;
        data[0] = nullptr;
        data[1] = nullptr;

        return *this;
    }
};


template <typename T>
struct AtomicNodePtr
{
    AtomicNode* node { nullptr };
    T*          data { nullptr };
};


BEE_CORE_API AtomicNode* make_atomic_node(Allocator* allocator, const size_t data_size);

template <typename T>
BEE_FORCE_INLINE AtomicNode* atomic_node_cast(T* data)
{
    return reinterpret_cast<AtomicNode*>(reinterpret_cast<u8*>(data) - sizeof(AtomicNode));
}

template <typename T, typename... Args>
AtomicNodePtr<T> make_atomic_node(Allocator* allocator, Args&&... args)
{
    auto node = make_atomic_node(allocator, sizeof(T));
    auto data = static_cast<T*>(node->data[0]);

    new (data) T(std::forward<Args>(args)...);

    AtomicNodePtr<T> ptr{};
    ptr.node = node;
    ptr.data = data;
    return ptr;
}

/*
 ****************************************
 *
 * # AtomicStack
 *
 * Inspired by the implementation used
 * by the Golang runtime
 * (https://github.com/golang/go/blob/master/src/runtime/lfstack.go)
 *
 ****************************************
 */
class AtomicStack
{
public:
    AtomicStack() = default;

    AtomicStack(AtomicStack&& other) noexcept
    {
        head_.store(other.head_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        other.head_.store(0, std::memory_order_relaxed);
    }

    AtomicStack& operator=(AtomicStack&& other) noexcept
    {
        head_.store(other.head_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        other.head_.store(0, std::memory_order_seq_cst);
        return *this;
    }

    void push(AtomicNode* node)
    {
        // increment ABA guard count value
        ++node->version;

        const auto new_link = pack_node(node, node->version);
        BEE_ASSERT_F(unpack_node(new_link) == node, "Packed node was invalid: this is a fatal error with AtomicStack");

        auto old_link = head_.load(std::memory_order_seq_cst);
        do
        {
            node->next.store(old_link, std::memory_order_seq_cst);
        } while (!head_.compare_exchange_weak(old_link, new_link, std::memory_order_seq_cst));
    }

    AtomicNode* pop()
    {
        AtomicNode* result = nullptr;
        u64 next_link = 0;
        u64 old_link = head_.load(std::memory_order_seq_cst);

        do
        {
            if (old_link == 0)
            {
                return nullptr;
            }

            result = unpack_node(old_link);
            next_link = result->next.load(std::memory_order_seq_cst);
        } while (!head_.compare_exchange_weak(old_link, next_link, std::memory_order_seq_cst));

        return result;
    }

    bool empty() const
    {
        return head_.load(std::memory_order_relaxed) == 0;
    }

private:
    std::atomic<u64> head_ { 0 };

#ifdef BEE_ARCH_64BIT
    static constexpr u64 one_ = static_cast<u64>(1);

    /*
     * 64-bit addresses on x86_64 are 48-bit numbers sign-extended to 64-bits. We don't need the sign
     * so we can left by 48-bits and use the bottom 16 bits to store the nodes count
     */
    static constexpr u64 address_bits_ = 48u;
    static constexpr u64 address_shift_ = 64u - address_bits_;

    /*
     * We can also take advantage of the fact that allocated memory is always going to be pointer-aligned
     * and use the bottom unused 3 bits (since they're always going to be on 8-byte boundaries)
     * to get a total 19 bits of count storage in the node (16 + 3)
     */
    static constexpr u64 spare_align_bits_ = 3u;
    static constexpr u64 count_bits_ = 64u - address_bits_ + spare_align_bits_;
    static constexpr u64 count_mask_ = (one_ << count_bits_) - one_;

    static u64 pack_node(AtomicNode* node, const u64 count)
    {
        // shift left to remove the sign-extension and pack with the count in the lower 19 bits
        const auto node_as_int = static_cast<u64>(reinterpret_cast<uintptr_t>(node));
        return (node_as_int << address_shift_) | (count & count_mask_);
    }

    static AtomicNode* unpack_node(const u64 value)
    {
        // as we removed the sign extension in packing the node we need to reapply it by casting to i64
        const auto ptr = static_cast<uintptr_t>(static_cast<i64>(value));
        return reinterpret_cast<AtomicNode*>(ptr >> count_bits_ << spare_align_bits_);
    }
#else
    // 32-bit implementation can store the full pointer (4 bytes) with a 32-bit count in the one u64
    static constexpr address_bits_ = 32u;

    static u64 pack_node(AtomicNode* node, const u64 count)
    {
        const auto node_as_int = static_cast<u64>(reinterpret_cast<uintptr_t>(node));
        return (node_as_int << address_bits_) | count;
    }

    static AtomicNode* unpack_node(const u64 value)
    {
        return reinterpret_cast<AtomicNode*>(static_cast<uintptr_t>(value >> address_bits_));
    }
#endif // BEE_ARCH_64BIT
};


} // namespace bee