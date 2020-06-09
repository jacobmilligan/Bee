/*
 *  Hardware.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Error.hpp"
#include "Bee/Core/Concurrency.hpp"

#define CPU_INFO_IMPLEMENTATION
#include <cpu_info.h>

namespace bee {
namespace concurrency {


static const cpui_result& get_cpu_info()
{
    static bool initialized = false;
    static cpui_result global_cpu_info{};

    if (!initialized) {
        const auto cpui_error = cpui_get_info(&global_cpu_info);
        BEE_ASSERT_F(cpui_error == CPUI_SUCCESS, "Failed to get CPU info with error: %d", cpui_error);
        initialized = true;
    }

    return global_cpu_info;
}


u32 physical_core_count()
{
    return get_cpu_info().physical_cores;
}

u32 logical_core_count()
{
    return get_cpu_info().logical_cores;
}


} // namespace concurrency


void SpinLock::lock()
{
    while (lock_.test_and_set(std::memory_order_acquire)) {}
}

void SpinLock::unlock()
{
    lock_.clear(std::memory_order_release);
}

RecursiveSpinLock::RecursiveSpinLock() noexcept
{
    unlock_and_reset();
}

RecursiveSpinLock::~RecursiveSpinLock()
{
    unlock_and_reset();
}

void RecursiveSpinLock::unlock_and_reset()
{
    owner_.store(limits::max<thread_id_t>(), std::memory_order_release);
    lock_count_.store(0, std::memory_order_release);
    lock_.unlock();
}

void RecursiveSpinLock::lock()
{
    if (current_thread::id() == owner_.load(std::memory_order_acquire))
    {
        lock_count_.fetch_add(1, std::memory_order_release);
        return;
    }

    lock_.lock();
    owner_.store(current_thread::id(), std::memory_order_release);
    lock_count_.store(1, std::memory_order_release);
}

void RecursiveSpinLock::unlock()
{
    if (owner_.load(std::memory_order_acquire) != current_thread::id())
    {
        return;
    }

    const auto lock_count = lock_count_.fetch_sub(1, std::memory_order_release);

    // fetch_sub returns the value it was set to *before* the subtraction
    if (lock_count <= 1)
    {
        unlock_and_reset();
    }
}


AtomicNode* make_atomic_node(Allocator* allocator, const size_t data_size)
{
    auto* ptr = static_cast<u8*>(BEE_MALLOC_ALIGNED(allocator, sizeof(AtomicNode) + data_size, 64));
    auto* node = reinterpret_cast<AtomicNode*>(ptr);

    new (node) AtomicNode{};

    node->data[0] = ptr + sizeof(AtomicNode);

    return node;
}



} // namespace bee
