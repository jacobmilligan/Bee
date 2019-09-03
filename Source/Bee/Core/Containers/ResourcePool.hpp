/*
 *  ResourcePool.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Noncopyable.hpp"
#include "Bee/Core/Error.hpp"
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Handle.hpp"

#include <functional>

namespace bee {


enum class ResourceReleaseMode {
    retain,
    destruct
};

/**
 * A `ResourcePool` pools concrete resources that are retrieved and allocated using `VersionedHandle` structs.
 * Resource pools are most often used to store hidden or platform-specific resources that the user can
 * reference through opaque handles rather than using opaque pointers. By using integer-based handles
 * instead of pointer-based ones, the resource pool can assert on invalid indices and stale handle versions
 * while also guarantee that no issues relating to undefined memory access or nullptr dereferencing will
 * occur. Memory allocated by a `ResourcePool` is stored on the stack by default in a static c-array.
 *
 * For more reading on integer-based handles vs pointer-based ones, see:
 * https://floooh.github.io/2018/06/17/handles-vs-pointers.html
 *
 * @tparam Capacity The pool size
 * @tparam HandleType The type of handle used to access resources, i.e. BEE_DEFINE_VERSIONED_HANDLE(AnObject), HandleType == AnObjectHandle
 * @tparam ResourceType The actual stored resource type
 */
template <u32 Capacity, typename HandleType, typename ResourceType>
class ResourcePool : Noncopyable
{
private:
    struct Resource
    {
        bool            active { false };
        u32             version { handle_t::min_version };
        ResourceType    data;
    };

public:
    static constexpr u32 capacity = Capacity;
    static constexpr u32 index_end = Capacity + 1;

    using pool_t        = ResourcePool<Capacity, HandleType, ResourceType>;
    using handle_t      = HandleType;
    using resource_t    = ResourceType;

    static_assert(
        math::is_power_of_two(capacity),
        "Skyrocket: ResourcePool<Capacity, HandleType, ResourceType>: Capacity must be a power of two"
    );

    static_assert(
        std::is_base_of_v<VersionedHandle<typename handle_t::tag_t>, handle_t>,
        "Skyrocket: ResourcePool<Capacity, HandleType, ResourceType>: HandleType must derive from "
        "VersionedHandle, i.e:\nstruct MyHandle : public VersionedHandle<MyHandle>{}"
    );

    static_assert(
        Capacity < handle_t::index_mask - 1,
        "Skyrocket: ResourcePool: Capacity must be less than 2^24 - 1"
    );

    class iterator
    {
    public:
        using value_type        = resource_t;
        using difference_type   = ptrdiff_t;
        using reference         = resource_t&;
        using pointer           = resource_t*;

        explicit iterator(pool_t* pool, const u32 index)
            : pool_(pool),
              resources_(pool->resources_.data()),
              resource_idx_(index)
        {}

        iterator(const iterator& other)
            : pool_(other.pool_),
              resources_(other.resources_),
              resource_idx_(other.resource_idx_)
        {}

        iterator& operator=(const iterator& other)
        {
            pool_ = other.pool_;
            resources_ = other.resources_;
            resource_idx_ = other.resource_idx_;
            return *this;
        }

        bool operator==(const iterator& other) const
        {
            return pool_ == other.pool_
                && resources_ == other.resources_
                && resource_idx_ == other.resource_idx_;
        }

        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }

        reference operator*() const
        {
            return resources_[resource_idx_].data;
        }

        pointer operator->() const
        {
            return &resources_[resource_idx_].data;
        }

        const iterator operator++(int)
        {
            iterator result(*this);
            ++*this;
            return result;
        }

        iterator& operator++()
        {
            do
            {
                // skip over inactive items
                ++resource_idx_;
                if (resources_[resource_idx_].active)
                {
                    break;
                }
            } while (resource_idx_ < pool_->capacity);

            return *this;
        }
    private:
        pool_t*     pool_ { nullptr };
        Resource*   resources_ { nullptr };
        u32         resource_idx_ { handle_t::min_id };
    };

    ResourcePool(Allocator* allocator = system_allocator());

    ~ResourcePool();

    ResourcePool(pool_t&& other) noexcept;

    pool_t& operator=(pool_t&& other) noexcept;

    void reset();

    void clear();

    handle_t allocate();

    void deallocate(const handle_t& handle, const ResourceReleaseMode deallocation_mode = ResourceReleaseMode::retain);

    inline u32 allocated_count() const
    {
        return next_alloc_idx_ - next_dealloc_idx_;
    }

    inline constexpr bool empty() const
    {
        return next_alloc_idx_ == next_dealloc_idx_;
    }

    inline constexpr bool full() const
    {
        return next_alloc_idx_ == next_dealloc_idx_ + capacity;
    }

    bool is_active(const HandleType& handle) const;

    ResourceType* operator[](const handle_t& handle);

    const ResourceType* operator[](const handle_t& handle) const;

    iterator begin();

    iterator end();
private:
    static constexpr u32 free_list_mask_ = capacity - 1;

    u32                     next_alloc_idx_ { 0 };
    u32                     next_dealloc_idx_ { 0 };
    FixedArray<Resource>    resources_;
    FixedArray<u32>         free_list_;

    const ResourceType* get(const handle_t& handle) const;
    constexpr bool index_range_is_valid(const handle_t& handle) const;
    void move_construct(pool_t&& other) noexcept;
};

template <u32 Capacity, typename HandleType, typename ResourceType>
ResourcePool<Capacity, HandleType, ResourceType>::ResourcePool(Allocator* allocator)
    : resources_(FixedArray<Resource>::with_size(index_end, allocator)),
      free_list_(capacity, 0, allocator)
{
    reset();
}

template <u32 Capacity, typename HandleType, typename ResourceType>
ResourcePool<Capacity, HandleType, ResourceType>::~ResourcePool()
{
    // FixedArray will take care of deallocation of memory - just reset indices
    next_alloc_idx_ = 0;
    next_dealloc_idx_ = 0;
}

template <u32 Capacity, typename HandleType, typename ResourceType>
ResourcePool<Capacity, HandleType, ResourceType>::ResourcePool(
    ResourcePool<Capacity, HandleType, ResourceType>&& other
) noexcept
{
    move_construct(std::move(other));
}

template <u32 Capacity, typename HandleType, typename ResourceType>
void ResourcePool<Capacity, HandleType, ResourceType>::move_construct(
    ResourcePool<Capacity, HandleType, ResourceType>&& other
) noexcept
{
    resources_ = other.resources_;
    free_list_ = other.free_list_;
    next_alloc_idx_ = other.next_alloc_idx_;
    next_dealloc_idx_ = other.next_dealloc_idx_;

    other.next_alloc_idx_ = 0;
    other.next_dealloc_idx_ = 0;
}

template <u32 Capacity, typename HandleType, typename ResourceType>
ResourcePool<Capacity, HandleType, ResourceType>&
ResourcePool<Capacity, HandleType, ResourceType>::operator=(
    ResourcePool<Capacity, HandleType, ResourceType>&& other
) noexcept
{
    move_construct(std::move(other));
    return *this;
}

template <u32 Capacity, typename HandleType, typename ResourceType>
void ResourcePool<Capacity, HandleType, ResourceType>::clear()
{
    // index 0 is the invalid ID
    for (u32 id_idx = handle_t::min_id; id_idx < index_end; ++id_idx)
    {
        resources_[id_idx].active = false;
        resources_[id_idx].version = handle_t::min_version;
        resources_[id_idx].data.~ResourceType();
        // only indices > min_id are considered valid - the resource at index min_id is always the invalid handle
        free_list_[id_idx - handle_t::min_id] = id_idx;
    }

    // setup the invalid handle
    resources_[handle_t::invalid_id].active = false;
    resources_[handle_t::invalid_id].version = 0;

    next_alloc_idx_ = 0;
    next_dealloc_idx_ = 0;
}

template <u32 Capacity, typename HandleType, typename ResourceType>
void ResourcePool<Capacity, HandleType, ResourceType>::reset()
{
    // index 0 is the invalid ID
    for (u32 id_idx = handle_t::min_id; id_idx < index_end; ++id_idx)
    {
        resources_[id_idx].active = false;
        resources_[id_idx].version = handle_t::min_version;
        new(&resources_[id_idx].data) ResourceType{};
        // only indices > min_id are considered valid - the resource at index min_id is always the invalid handle
        free_list_[id_idx - handle_t::min_id] = id_idx;
    }

    // setup the invalid handle
    resources_[handle_t::invalid_id].active = false;
    resources_[handle_t::invalid_id].version = 0;

    next_alloc_idx_ = 0;
    next_dealloc_idx_ = 0;
}

template <u32 Capacity, typename HandleType, typename ResourceType>
HandleType ResourcePool<Capacity, HandleType, ResourceType>::allocate()
{
    if (BEE_FAIL_F(!full(), "Unable to allocate handles - pool is exhausted"))
    {
        return handle_t { handle_t::invalid_id };
    }

    const auto handle_idx = free_list_[next_alloc_idx_ & free_list_mask_];
    ++next_alloc_idx_;

    BEE_ASSERT(handle_idx != handle_t::invalid_id);

    resources_[handle_idx].active = true;
    // version sits in the upper 8 bits - the remaining 24 bits are the index
    return handle_t(handle_idx, resources_[handle_idx].version);
//    return handle_t {(resources_[handle_idx].version << handle_t::index_bits) | handle_idx};
}

template <u32 Capacity, typename HandleType, typename ResourceType>
void ResourcePool<Capacity, HandleType, ResourceType>::deallocate(
    const HandleType& handle,
    const ResourceReleaseMode deallocation_mode
)
{
    BEE_ASSERT(handle.is_valid());
    if (BEE_FAIL_F(!empty(), "Failed to deallocate a handle - the pool has no allocations to recycle"))
    {
        return;
    }

    const auto resource_idx = handle.index();

    if (BEE_FAIL_F(handle.version() == resources_[resource_idx].version, "Attempted to free a resource using an outdated handle"))
    {
        return;
    }

    resources_[resource_idx].active = false;
    resources_[resource_idx].version++;
    free_list_[next_dealloc_idx_ & free_list_mask_] = resource_idx;
    ++next_dealloc_idx_;

    if (deallocation_mode == ResourceReleaseMode::destruct) {
        resources_[resource_idx].data.~ResourceType();
    }
}

template <u32 Capacity, typename HandleType, typename ResourceType>
constexpr bool ResourcePool<Capacity, HandleType, ResourceType>::index_range_is_valid(const handle_t& handle) const
{
    const auto index = handle.index();
    // must be less than the amount allocated and total capacity but greater than the invalid handle's index
    return index < index_end && index >= handle_t::min_id;
}

template <u32 Capacity, typename HandleType, typename ResourceType>
const ResourceType* ResourcePool<Capacity, HandleType, ResourceType>::get(const handle_t& handle) const
{

    if (BEE_FAIL_F(index_range_is_valid(handle), "Handle had an invalid index"))
    {
        return nullptr;
    }

    const auto index = handle.index();

    if (BEE_FAIL_F(resources_[index].active, "Handle referenced a deallocated resource"))
    {
        return nullptr;
    }

    if (BEE_FAIL_F(handle.version() == resources_[index].version, "Handle was out of date with the version stored in the resource pool"))
    {
        return nullptr;
    }

    return &resources_[index].data;
}

template <u32 Capacity, typename HandleType, typename ResourceType>
bool ResourcePool<Capacity, HandleType, ResourceType>::is_active(const HandleType& handle) const
{
    // Invalid index range == invalid handle
    if (!index_range_is_valid(handle))
    {
        return false;
    }

    const auto index = handle.index();

    // versions are in sync
    return resources_[index].version == handle.version() && resources_[index].active;
}

template <u32 Capacity, typename HandleType, typename ResourceType>
ResourceType* ResourcePool<Capacity, HandleType, ResourceType>::operator[](const handle_t& handle)
{
    return const_cast<ResourceType*>(get(handle));
}

template <u32 Capacity, typename HandleType, typename ResourceType>
const ResourceType* ResourcePool<Capacity, HandleType, ResourceType>::operator[](const handle_t& handle) const
{
    return get(handle);
}

template <u32 Capacity, typename HandleType, typename ResourceType>
typename ResourcePool<Capacity, HandleType, ResourceType>::iterator
ResourcePool<Capacity, HandleType, ResourceType>::begin()
{
    for (u32 resource_idx = 0; resource_idx < capacity; ++resource_idx)
    {
        if (resources_[resource_idx].active)
        {
            return iterator(this, resource_idx);
        }
    }

    return end();
}

template <u32 Capacity, typename HandleType, typename ResourceType>
typename ResourcePool<Capacity, HandleType, ResourceType>::iterator
ResourcePool<Capacity, HandleType, ResourceType>::end()
{
    return iterator(this, capacity);
}


} // namespace bee
