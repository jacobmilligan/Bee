/*
 *  MemoryTracker.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Memory/MemoryTracker.hpp"

#include <inttypes.h>

namespace bee {
namespace memory_tracker {


struct Proxy
{
    static constexpr i32 stack_frame_count = 16;

    TrackingMode                            tracking_mode { TrackingMode::enabled };
    RecursiveSpinLock                       mutex;
    DynamicHashMap<void*, AllocationEvent>  allocations;
    isize                                   total_allocations { 0 };
    isize                                   peak_allocations { 0 };

    Proxy()
        : total_allocations(0),
          peak_allocations(0)
    {}

    ~Proxy()
    {
        // NOTE(Jacob): support detecting leaks on shut down?
        tracking_mode = TrackingMode::disabled;
        allocations.clear();
    }
};

static Proxy g_proxy; // the global allocator proxy context


/*
 **************************************
 *
 * # Allocator proxy - implementation
 *
 **************************************
 */
void set_tracking_mode(const TrackingMode mode)
{
    scoped_recursive_spinlock_t lock(g_proxy.mutex);

    if (mode == TrackingMode::disabled)
    {
        // Clear the current allocations so we don't get errors when re-enabling it but keep total allocations and
        // peak usage around because that is still valid
        g_proxy.allocations.clear();
    }

    g_proxy.tracking_mode = mode;
}

void record_manual_allocation(void* address, const size_t size, const size_t alignment, const i32 skipped_stack_frames)
{
    scoped_recursive_spinlock_t lock(g_proxy.mutex);

    if (g_proxy.tracking_mode == TrackingMode::disabled)
    {
        return;
    }

    // Temporarily suspend tracking to avoid recursion
    g_proxy.tracking_mode = TrackingMode::disabled;
    {
        BEE_ASSERT_F(address != nullptr, "Detected invalid allocation");
        BEE_ASSERT_F(g_proxy.allocations.find(address) == nullptr, "Detected memory overwrite");

        auto keyval = g_proxy.allocations.insert(address, AllocationEvent{});

        auto& event = keyval->value;
        event.address = address;
        event.size = size;
        event.alignment = alignment;
        capture_stack_trace(&event.stack_trace, Proxy::stack_frame_count, skipped_stack_frames + 1);

        BEE_ASSERT_F(g_proxy.total_allocations + sign_cast<isize>(size) < limits::max<isize>(), "Detected too many allocations");
        g_proxy.total_allocations += sign_cast<isize>(size);
    }
    g_proxy.tracking_mode = TrackingMode::enabled;
}

void erase_manual_allocation(void* address)
{
    scoped_recursive_spinlock_t lock(g_proxy.mutex);

    if (g_proxy.tracking_mode == TrackingMode::disabled)
    {
        return;
    }

    // Temporarily suspend tracking to avoid recursion
    g_proxy.tracking_mode = TrackingMode::disabled;
    {
        auto event = g_proxy.allocations.find(address);
        BEE_ASSERT_F(event != nullptr, "Detected double free");
        const auto alloc_size = event->value.size;

        g_proxy.allocations.erase(address);

        BEE_ASSERT_F(g_proxy.total_allocations - alloc_size >= 0, "Detected memory leak");
        g_proxy.total_allocations -= alloc_size;
    }
    g_proxy.tracking_mode = TrackingMode::enabled;

}

void* allocate_tracked(Allocator* allocator, const size_t size, const size_t alignment)
{
    scoped_recursive_spinlock_t lock(g_proxy.mutex);

    auto address = allocator->allocate(size, alignment);

    if (!allocator->allocator_proxy_disable_tracking())
    {
        record_manual_allocation(address, size, alignment, 1);
    }

    return address;
}

void* reallocate_tracked(Allocator* allocator, void* old_address, const size_t old_size, const size_t new_size, const size_t alignment)
{
    scoped_recursive_spinlock_t lock(g_proxy.mutex);

    auto new_address = allocator->reallocate(old_address, old_size, new_size, alignment);

    // Reallocating is a special-case where a nullptr is a useful return value for certain allocators
    if (new_address != nullptr && !allocator->allocator_proxy_disable_tracking())
    {
        erase_manual_allocation(old_address);
        record_manual_allocation(new_address, new_size, alignment, 1);
    }

    return new_address;
}

void deallocate_tracked(Allocator* allocator, void* ptr)
{
    scoped_recursive_spinlock_t lock(g_proxy.mutex);

    allocator->deallocate(ptr);

    // freeing a nullptr is considered valid and evaluates to a no-op
    if (ptr != nullptr && !allocator->allocator_proxy_disable_tracking())
    {
        erase_manual_allocation(ptr);
    }
}

i32 get_tracked_allocations(AllocationEvent* dst_buffer, const i32 dst_buffer_count)
{
    scoped_recursive_spinlock_t lock(g_proxy.mutex);

    if (dst_buffer == nullptr)
    {
        return g_proxy.allocations.size();
    }

    const auto copied_count = math::min(dst_buffer_count, g_proxy.allocations.size());

    int dst_idx = 0;
    for (const auto& alloc : g_proxy.allocations)
    {
        memcpy(dst_buffer + dst_idx, &alloc.value, sizeof(AllocationEvent));
        ++dst_idx;
    }
    return copied_count;
}

void log_tracked_allocations(const LogVerbosity verbosity)
{
    scoped_recursive_spinlock_t lock(g_proxy.mutex);

    log_write(
        verbosity,
        "Logging tracked allocations made via bee::Allocator interfaces.\n"
        "    Total allocated memory: %" PRIxPTR " bytes\n"
                                                "    Peak allocated memory: %" PRIxPTR " bytes",
        g_proxy.total_allocations,
        g_proxy.peak_allocations
    );

    DebugSymbol call_site{};

    for (const auto& event : g_proxy.allocations)
    {
        symbolize_stack_trace(&call_site, event.value.stack_trace, 1);

        log_write(
            verbosity,
            "%12zu bytes | %s:%d | functions: %s",
            event.value.size,
            call_site.filename,
            call_site.line,
            call_site.function_name
        );
    }
}


} // namespace memory_tracker
} // namespace bee
