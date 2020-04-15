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
class ResourcePool
{
public:
    using id_t = typename HandleType::generator_t::id_t;

private:
    struct ResourceChunk
    {
        id_t            index { 0 };
        id_t            size { 0 };
        id_t            capacity { 0 };
        u8*             ptr { nullptr };
        id_t*           free_list { nullptr };
        id_t*           versions { nullptr };
        bool*           active_states { nullptr };
        ResourceType*   data { nullptr };
    };
public:
    using handle_t      = HandleType;
    using resource_t    = ResourceType;

    static_assert(sizeof(id_t) <= 8,
        "Bee: ResourcePool<HandleType, ResourceType>: HandleType must be declared using the BEE_VERSIONED_HANDLE() "
        "macro and be smaller than 64 bits in size"
    );

    class iterator
    {
    public:
        using value_type        = ResourceType;
        using difference_type   = ptrdiff_t;
        using reference         = ResourceType&;
        using pointer           = ResourceType*;

        explicit iterator(ResourcePool* pool, const id_t index)
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
            const auto chunk_count = pool_->chunk_count_;
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
        ResourcePool*   pool_ { nullptr };
        id_t            current_index_ { 0 };
        id_t            current_chunk_ { 0 };
    };

    ResourcePool() = default;

    explicit ResourcePool(const size_t chunk_byte_size, Allocator* allocator = system_allocator())
        : chunk_byte_size_(chunk_byte_size),
          chunk_capacity_(chunk_byte_size / sizeof(ResourceType)),
          allocator_(allocator)
    {}

    ~ResourcePool()
    {
        for (id_t chunk = 0; chunk < chunk_count_; ++chunk)
        {
            reset_chunk(&chunks_[chunk]);
            if (chunks_[chunk].ptr != nullptr)
            {
                BEE_FREE(allocator_, chunks_[chunk].ptr);
                chunks_[chunk].ptr = nullptr;
            }
        }

        if (chunks_ != nullptr && allocator_ != nullptr)
        {
            BEE_FREE(allocator_, chunks_);
        }
    }

    template <typename... ConstructorArgs>
    HandleType allocate(ConstructorArgs&&... args)
    {
        if (next_free_resource_ >= chunk_count_ * chunk_capacity_)
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
        new (&chunk.data[chunk_index]) ResourceType(std::forward<ConstructorArgs>(args)...);

        return HandleType(index, chunk.versions[chunk_index]);
    }

    void deallocate(const HandleType& handle)
    {
        const auto index = handle.index();
        const auto index_in_chunk = index % chunk_capacity_;
        auto& chunk = get_chunk(index);

        BEE_ASSERT_F(index_in_chunk < chunk_capacity_, "Handle had an invalid index");
        BEE_ASSERT_F(chunk.versions[index_in_chunk] == handle.version(), "Attempted to free a resource using an outdated handle");
        BEE_ASSERT_F(chunk.active_states[index_in_chunk], "Handle referenced a deallocated resource");

        destruct(&chunk.data[index_in_chunk]);
        chunk.active_states[index_in_chunk] = false;
        chunk.versions[index_in_chunk] = (chunk.versions[index_in_chunk] + 1) & HandleType::generator_t::high_mask;
        chunk.versions[index_in_chunk] = math::min(chunk.versions[index_in_chunk], HandleType::generator_t::min_high);

        chunk.free_list[index_in_chunk] = next_free_resource_;
        next_free_resource_ = index;

        --resource_count_;
        --chunk.size;
    }

    void clear()
    {
        for (id_t chunk = 0; chunk < chunk_count_; ++chunk)
        {
            reset_chunk(&chunks_[chunk]);
        }
    }

    inline bool is_active(const HandleType& handle) const
    {
        const auto index = handle.index();
        return chunks_[index / chunk_capacity_].active_states[index % chunk_capacity_];
    }

    inline id_t size() const
    {
        return resource_count_;
    }

    inline id_t chunk_count() const
    {
        return chunk_count_;
    }

    inline size_t allocated_size() const
    {
        return chunk_byte_size_ * chunk_count_;
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
        return iterator(this, chunk_count_ * chunk_capacity_);
    }

    inline iterator get_iterator(const HandleType& handle)
    {
        BEE_ASSERT(handle.is_valid());
        return iterator(this, handle.index());
    }
private:

    size_t          chunk_byte_size_ { 0 };
    id_t            chunk_capacity_ { 0 };
    id_t            chunk_count_ { 0 };
    id_t            resource_count_ { 0 };
    id_t            next_free_resource_ { 0 };
    Allocator*      allocator_ { nullptr };
    ResourceChunk*  chunks_ { nullptr };

    void allocate_chunk()
    {
        auto new_chunks_ptr = BEE_REALLOC(allocator_, chunks_, chunk_count_ * sizeof(ResourceChunk), (chunk_count_ + 1) * sizeof(ResourceChunk), alignof(ResourceChunk));

        BEE_ASSERT(new_chunks_ptr != nullptr);

        chunks_ = static_cast<ResourceChunk*>(new_chunks_ptr);
        ++chunk_count_;

        auto chunk = chunks_ + chunk_count_ - 1;
        chunk->index = chunk_count_ - 1;
        chunk->size = 0;
        chunk->capacity = chunk_capacity_;
        chunk->ptr = static_cast<u8*>(BEE_MALLOC(allocator_, chunk_capacity_ * (sizeof(id_t) * 2 + sizeof(bool) + sizeof(ResourceType))));
        chunk->free_list = reinterpret_cast<id_t*>(chunk->ptr);
        chunk->versions = chunk->free_list + chunk_capacity_;
        chunk->active_states = reinterpret_cast<bool*>(chunk->versions + chunk_capacity_);
        chunk->data = reinterpret_cast<ResourceType*>(chunk->active_states + chunk_capacity_);

        memset(chunk->active_states, 0, sizeof(bool) * chunk_capacity_);

        for (id_t r = 0; r < chunk_capacity_; ++r)
        {
            new (chunk->data + r) ResourceType{};
        }

        reset_chunk(chunk);
    }

    void reset_chunk(ResourceChunk* chunk)
    {
        for (id_t i = 0; i < chunk->capacity; ++i)
        {
            chunk->free_list[i] = (chunk->index * chunk->capacity) + i + 1;
            chunk->versions[i] = 1;
            if (chunk->active_states[i])
            {
                chunk->active_states[i] = false;
                destruct(chunk->data + i);
            }
        }
    }

    ResourceChunk& get_chunk(const id_t index)
    {
        return chunks_[index / chunk_capacity_];
    }

    const ResourceType& validate_resource(const HandleType& handle) const
    {
        const auto index = handle.index();
        const auto chunk_index = index / chunk_capacity_;
        const auto resource_index = index % chunk_capacity_;
        BEE_ASSERT_F(resource_count_ > 0 && chunk_index < chunk_count_ && resource_index < chunk_capacity_, "Handle had an invalid index");
        BEE_ASSERT_F(chunks_[chunk_index].versions[resource_index] == handle.version(), "Handle was out of date with the version stored in the resource pool");
        BEE_ASSERT_F(chunks_[chunk_index].active_states[resource_index], "Handle referenced a deallocated resource");
        return chunks_[chunk_index].data[resource_index];
    }
};


template <typename HandleType, typename ResourceType>
class ThreadSafeResourcePool
{
private:
    using id_t = typename HandleType::generator_t::id_t;

public:
    using iterator_t = typename ResourcePool<HandleType, ResourceType>::iterator;

    ThreadSafeResourcePool() = default;

    explicit ThreadSafeResourcePool(const size_t chunk_byte_size, Allocator* allocator = system_allocator())
        : pool_(chunk_byte_size, allocator)
    {}

    template <typename... ConstructorArgs>
    HandleType allocate(ConstructorArgs&&... args)
    {
        scoped_recursive_spinlock_t lock(mutex_);
        return pool_.allocate(std::forward<ConstructorArgs>(args)...);
    }

    void deallocate(const HandleType& handle)
    {
        scoped_recursive_spinlock_t lock(mutex_);
        pool_.deallocate(handle);
    }

    void clear()
    {
        scoped_recursive_spinlock_t lock(mutex_);
        pool_.clear();
    }

    inline bool is_active(const HandleType& handle) const
    {
        scoped_recursive_spinlock_t lock(mutex_);
        return pool_.is_active(handle);
    }

    inline id_t size() const
    {
        scoped_recursive_spinlock_t lock(mutex_);
        return resource_count_;
    }

    inline id_t chunk_count() const
    {
        scoped_recursive_spinlock_t lock(mutex_);
        return chunk_count_;
    }

    inline size_t allocated_size() const
    {
        scoped_recursive_spinlock_t lock(mutex_);
        return chunk_byte_size_ * chunk_count_;
    }

    inline ResourceType& operator[](const HandleType& handle)
    {
        scoped_recursive_spinlock_t lock(mutex_);
        return pool_[handle];
    }

    inline const ResourceType& operator[](const HandleType& handle) const
    {
        scoped_recursive_spinlock_t lock(mutex_);
        return pool_[handle];
    }

    inline iterator_t begin()
    {
        scoped_recursive_spinlock_t lock(mutex_);
        return iterator_t(this, 0);
    }

    inline iterator_t end()
    {
        scoped_recursive_spinlock_t lock(mutex_);
        return iterator_t(this, chunk_count_ * chunk_capacity_);
    }

    inline iterator_t get_iterator(const HandleType& handle)
    {
        BEE_ASSERT(handle.is_valid());
        scoped_recursive_spinlock_t lock(mutex_);
        return iterator_t(this, handle.index());
    }
private:
    RecursiveSpinLock                       mutex_;
    ResourcePool<HandleType, ResourceType>  pool_;
};


} // namespace bee