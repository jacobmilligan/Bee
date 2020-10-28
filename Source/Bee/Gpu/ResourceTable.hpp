/*
 *  HandleTable2.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Plugins/Gpu/Gpu.hpp"


namespace bee {

class GpuResourceTable
{
public:
    static constexpr i32 capacity = 1u << 24u;
    static constexpr i32 chunk_size = 4096;

    void init(const i32 thread_index)
    {
        memset(chunk_counts_, 0, static_array_length(chunk_counts_) * sizeof(i32));
        memset(chunks_, 0, static_array_length(chunks_) * sizeof(Entry*));
        next_ = 0;
        last_ = -1;
        thread_ = limits::max<u32>();
        remove_list_end_ = remove_list_begin_ = nullptr;
        thread_ = sign_cast<u32>(thread_index);
    }

    // thread-local
    GpuObjectHandle add(void* ptr)
    {
        const i32 index = next_;

        BEE_ASSERT(index < capacity);

        const i32 chunk_index = get_chunk_index(index);

        if (chunks_[chunk_index] == nullptr)
        {
            chunks_[chunk_index] = static_cast<Entry*>(BEE_MALLOC_ALIGNED(
                system_allocator(),
                chunk_size,
                alignof(Entry)
            ));

            for (int i = 0; i < chunk_capacity; ++i)
            {
                chunks_[chunk_index][i].chunk = chunk_index;
                chunks_[chunk_index][i].index = chunk_index * chunk_capacity + i;
                chunks_[chunk_index][i].version = 1;
                chunks_[chunk_index][i].next = chunks_[chunk_index][i].index + 1;
                chunks_[chunk_index][i].ptr = nullptr;
                chunks_[chunk_index][i].remove_list_next = nullptr;
            }

            if (last_ >= 0)
            {
                auto& last_resource = get_entry(last_);
                last_resource.next = chunk_index * chunk_capacity;
            }

            const i32 last_in_chunk = chunk_capacity - 1;

            last_ = chunk_index * chunk_capacity + last_in_chunk;
            chunks_[chunk_index][last_in_chunk].next = -1;
        }

        auto& resource = chunks_[chunk_index][get_resource_index(index)];
        resource.ptr = ptr;
        next_ = resource.next;
        ++chunk_counts_[chunk_index];

        const u32 handle = internal_handle_generator_t::make_handle(index, resource.version);
        return GpuObjectHandle(handle, thread_);
    }

    // free-threaded
    void* remove(const GpuObjectHandle handle)
    {
        BEE_ASSERT(handle.thread() == thread_);

        const i32 index = sign_cast<i32>(internal_handle_generator_t::get_low(handle.value()));
        const i32 version = sign_cast<i32>(internal_handle_generator_t::get_high(handle.value()));
        const i32 chunk_index = get_chunk_index(index);

        BEE_ASSERT(chunks_[chunk_index] != nullptr);

        auto& resource = chunks_[chunk_index][get_resource_index(index)];

        BEE_ASSERT(resource.version == version);

        ++resource.version;

        resource.remove_list_next = nullptr;

        if (remove_list_begin_ == nullptr)
        {
            remove_list_begin_ = remove_list_end_ = &resource;
        }
        else
        {
            remove_list_end_->remove_list_next = &resource;
            remove_list_end_ = &resource;
        }

        return resource.ptr;
    }

    // thread-unsafe
    void flush_removed()
    {
        auto* entry = remove_list_begin_;

        while (entry != nullptr)
        {
            // deferred removals have already had their version incremented
            const i32 chunk_index = entry->chunk;
            entry->ptr = nullptr;
            entry->next = last_;

            // if last == -1 the table is empty
            if (last_ >= 0)
            {
                get_entry(last_).next = entry->index;
            }

            entry = entry->remove_list_next;

            // book-keeping for the chunk info
            --chunk_counts_[chunk_index];
            if (chunk_counts_[chunk_index] <= 0)
            {
                // free the chunk if this is the last resource
                BEE_FREE(system_allocator(), chunks_[chunk_index]);
                chunks_[chunk_index] = nullptr;
                chunk_counts_[chunk_index] = 0;
            }
        }

        remove_list_begin_ = remove_list_end_ = nullptr;
    }

    // free-threaded
    inline void* get(const GpuObjectHandle handle)
    {
        BEE_ASSERT(handle.thread() == thread_);

        const i32 index = sign_cast<i32>(internal_handle_generator_t::get_low(handle.value()));
        const i32 version = sign_cast<i32>(internal_handle_generator_t::get_high(handle.value()));
        auto& entry = get_entry(index);

        BEE_ASSERT(entry.version == version);

        return entry.ptr;
    }

private:
    struct Entry
    {
        i32     chunk { -1 };
        i32     index { -1 };
        i32     version { 0 };
        i32     next { 0 };
        void*   ptr { nullptr };
        Entry*  remove_list_next { nullptr };
    };


    using internal_handle_generator_t = bee::HandleGenerator<u32, 18u, 6u>;

    static constexpr i32 max_chunks = (capacity * sizeof(Entry)) / chunk_size;
    static constexpr i32 chunk_capacity = chunk_size / sizeof(Entry);

    i32         chunk_counts_[max_chunks];
    Entry*      chunks_[max_chunks];
    i32         next_;
    i32         last_;
    u32         thread_;
    Entry*      remove_list_begin_;
    Entry*      remove_list_end_;

    constexpr i32 get_chunk_index(const i32 index)
    {
        return index / chunk_size;
    }

    constexpr i32 get_resource_index(const i32 index)
    {
        return index % chunk_capacity;
    }

    Entry& get_entry(const i32 index)
    {
        const i32 chunk = get_chunk_index(index);
        const i32 resource = get_resource_index(index);
        BEE_ASSERT(chunks_[chunk] != nullptr);
        return chunks_[chunk][resource];
    }
};


} // namespace bee