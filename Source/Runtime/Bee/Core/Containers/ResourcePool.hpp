/*
 *  ResourcePool.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/Containers/Array.hpp"

namespace bee {


template <typename HandleType, typename ResourceType>
class BEE_CORE_API ResourcePool
{
private:
    struct ResourceChunk
    {
        i32                         size { 0 };
        FixedArray<i32>             free_list;
        FixedArray<u32>             versions;
        FixedArray<bool>            active_states;
        FixedArray<ResourceType>    data;
    };
public:
    using handle_t      = HandleType;
    using resource_t    = ResourceType;

    static_assert(
        std::is_base_of_v<VersionedHandle<typename HandleType::tag_t>, HandleType>,
        "Bee: ResourcePool<Capacity, HandleType, ResourceType>: HandleType must derive from "
        "VersionedHandle, i.e:\nstruct MyHandle : public VersionedHandle<MyHandle>{}"
    );

    class iterator
    {
    public:
        using value_type        = ResourceType;
        using difference_type   = ptrdiff_t;
        using reference         = ResourceType&;
        using pointer           = ResourceType*;

        explicit iterator(ResourcePool* pool, const u32 index)
            : pool_(pool),
              current_index_(index % pool->chunk_capacity_),
              current_chunk_(index / pool->chunk_capacity_)
        {}

        iterator(const iterator& other)
            : pool_(other.pool_),
              current_index_(other.current_index_),
              current_chunk_(other.current_chunk_)
        {}

        iterator& operator=(const iterator& other)
        {
            pool_ = other.pool_;
            current_chunk_ = other.current_chunk_;
            current_index_ = other.current_index_;
            return *this;
        }

        bool operator==(const iterator& other) const
        {
            return pool_ == other.pool_
                && current_chunk_ == other.current_chunk_
                && current_index_ == other.current_index_;
        }

        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }

        reference operator*() const
        {
            return pool_->chunks_[current_chunk_].data[current_index_];
        }

        pointer operator->() const
        {
            return &pool_->chunks_[current_chunk_].data[current_index_];
        }

        const iterator operator++(int)
        {
            iterator result(*this);
            ++*this;
            return result;
        }

        iterator& operator++()
        {
            const auto chunk_count = pool_->chunks_.size();
            const auto chunk_capacity = pool_->chunk_capacity_;

            for (; current_chunk_ < chunk_count; ++current_chunk_)
            {
                if (pool_->chunks_[current_chunk_].size > 0)
                {
                    // skip over inactive items
                    for (; current_index_ < pool_->chunk_capacity_; ++current_index_)
                    {
                        if (pool_->chunks_[current_chunk_].active_states[current_index_])
                        {
                            ++current_index_;

                            if (current_index_ >= chunk_capacity)
                            {
                                current_index_ = 0;
                                ++current_chunk_;
                            }

                            return *this;
                        }
                    }
                }

                current_index_ = 0;
            }

            return *this;
        }
    private:
        ResourcePool*   pool_ {nullptr };
        i32             current_index_ { 0 };
        i32             current_chunk_ { 0 };
    };

    explicit ResourcePool(const size_t chunk_byte_size, Allocator* allocator = system_allocator())
        : chunk_byte_size_(chunk_byte_size),
          chunk_capacity_(chunk_byte_size / sizeof(ResourceType)),
          allocator_(allocator),
          chunks_(allocator)
    {}

    HandleType allocate()
    {
        if (next_free_resource_ >= chunks_.size() * chunk_capacity_)
        {
            allocate_chunk();
        }

        const auto index = next_free_resource_++;
        auto& chunk = get_chunk(index);
        const auto chunk_index = index % chunk_capacity_;

        ++chunk.size;
        ++resource_count_;

        next_free_resource_ = chunk.free_list[chunk_index];

        chunk.active_states[chunk_index] = true;
        new (&chunk.data[chunk_index]) ResourceType();

        return HandleType(index, chunk.versions[chunk_index]);
    }

    void deallocate(const HandleType& handle)
    {
        const auto index = sign_cast<i32>(handle.index());
        const auto chunk_index = index % chunk_capacity_;
        auto& chunk = get_chunk(index);

        BEE_ASSERT_F(index < resource_count_ && chunk_index < chunk_capacity_, "Handle had an invalid index");
        BEE_ASSERT_F(chunk.versions[chunk_index] == handle.version(), "Attempted to free a resource using an outdated handle");
        BEE_ASSERT_F(chunk.active_states[chunk_index], "Handle referenced a deallocated resource");

        destruct(&chunk.data[chunk_index]);
        chunk.active_states[chunk_index] = false;
        ++chunk.versions[chunk_index];

        chunk.free_list[chunk_index] = next_free_resource_;
        next_free_resource_ = index;

        --resource_count_;
        --chunk.size;
    }

    void clear()
    {
        for (auto& chunk : chunks_)
        {
            chunk.size = 0;
            chunk.free_list.clear();
            chunk.data.clear();
            chunk.versions.clear();
        }
    }

    void shrink_to_fit()
    {
        int new_size = chunks_.size();

        for (; new_size >= 0; --new_size)
        {
            if (chunks_[new_size].size > 0)
            {
                break;
            }
        }

        new_size = math::max(0, new_size);
        if (new_size != chunks_.size())
        {
            chunks_.resize(new_size);
            chunks_.shrink_to_fit();
        }
    }

    inline bool is_active(const HandleType& handle) const
    {
        const auto index = handle.index();
        return chunks_[index / chunk_capacity_].active_states[index % chunk_capacity_];
    }

    inline i32 size() const
    {
        return resource_count_;
    }

    inline i32 chunk_count() const
    {
        return chunks_.size();
    }

    inline size_t allocated_size() const
    {
        return chunk_byte_size_ * chunks_.size();
    }

    inline ResourceType& operator[](const HandleType& handle)
    {
        return const_cast<ResourceType&>(validate_resource(handle));
    }

    inline const ResourceType& operator[](const HandleType& handle) const
    {
        return validate_resource(handle);
    }

    inline iterator begin()
    {
        return iterator(this, 0);
    }

    inline iterator end()
    {
        return iterator(this, chunks_.size() * chunk_capacity_);
    }
private:

    size_t                      chunk_byte_size_ { 0 };
    i32                         chunk_capacity_ { 0 };
    i32                         resource_count_ { 0 };
    i32                         next_free_resource_ { 0 };
    Allocator*                  allocator_ { nullptr };
    FixedArray<ResourceChunk>   chunks_ { nullptr };

    void allocate_chunk()
    {
        chunks_.resize(chunks_.size() + 1);
        chunks_.back().size = 0;
        chunks_.back().free_list = FixedArray<i32>::with_size(chunk_capacity_, allocator_);
        chunks_.back().versions = FixedArray<u32>::with_size(chunk_capacity_, allocator_);
        chunks_.back().active_states = FixedArray<bool>::with_size(chunk_capacity_, allocator_);
        chunks_.back().data = FixedArray<ResourceType>::with_size(chunk_capacity_, allocator_);

        reset_chunk(chunks_.size() - 1);
    }

    void reset_chunk(const i32 chunk_index)
    {
        auto& chunk = chunks_[chunk_index];

        for (int i = 0; i < chunk.free_list.size(); ++i)
        {
            chunk.free_list[i] = (chunk_index * chunk_capacity_) + i + 1;
            chunk.versions[i] = 1;
            if (chunk.active_states[i])
            {
                chunk.active_states[i] = false;
                destruct(&chunk.data[i]);
            }
        }
    }

    ResourceChunk& get_chunk(const i32 index)
    {
        return chunks_[index / chunk_capacity_];
    }

    const ResourceType& validate_resource(const HandleType& handle) const
    {
        const auto index = sign_cast<i32>(handle.index());
        const auto chunk_index = index / chunk_capacity_;
        const auto resource_index = index % chunk_capacity_;
        BEE_ASSERT_F(index < resource_count_ && chunk_index < chunks_.size() && resource_index < chunk_capacity_, "Handle had an invalid index");
        BEE_ASSERT_F(chunks_[chunk_index].versions[resource_index] == handle.version(), "Handle was out of date with the version stored in the resource pool");
        BEE_ASSERT_F(chunks_[chunk_index].active_states[resource_index], "Handle referenced a deallocated resource");
        return chunks_[chunk_index].data[resource_index];
    }
};


} // namespace bee