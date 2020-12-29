/*
 *  HandleTable2.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/ResourcePool.hpp"
#include "Bee/Gpu/Gpu.hpp"


namespace bee {


BEE_VERSIONED_HANDLE_32(GpuObjectHandle);

template <typename HandleType, typename ValueType>
struct GpuResourceTable
{
    u32                                         thread { limits::max<u32>() };
    ResourcePool<GpuObjectHandle, ValueType>    pool;
    FixedArray<DynamicArray<GpuObjectHandle>>   pending_deallocations;

    GpuResourceTable() = default;

    GpuResourceTable(const i32 thread_index, const size_t chunk_byte_size, Allocator* allocator = system_allocator())
        : thread(static_cast<u32>(thread_index)),
          pool(chunk_byte_size, allocator)
    {
        pending_deallocations.resize(job_system_worker_count());
    }

    template <typename... ConstructorArgs>
    HandleType allocate(ConstructorArgs&&... args)
    {
        const auto handle = pool.allocate(BEE_FORWARD(args)...);
        return HandleType { handle.id, thread };
    }

    ValueType& deallocate(const HandleType handle)
    {
        BEE_ASSERT(handle.thread() == thread);
        const GpuObjectHandle local_handle(static_cast<u32>(handle.value()));
        pending_deallocations[job_worker_id()].push_back(local_handle);
        return pool[local_handle];
    }

    ValueType& operator[](const HandleType handle)
    {
        BEE_ASSERT(handle.thread() == thread);
        return pool[GpuObjectHandle { static_cast<u32>(handle.value()) }];
    }

    void flush_deallocations()
    {
        for (auto& dealloc_thread : pending_deallocations)
        {
            for (auto& handle : dealloc_thread)
            {
                pool.deallocate(handle);
            }

            dealloc_thread.clear();
        }
    }
};


} // namespace bee