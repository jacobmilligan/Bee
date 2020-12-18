/*
 *  HandleTable.inl
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"

namespace bee {



#define BEE_HANDLE_TABLE_VALIDATE(handle)       \
    BEE_BEGIN_MACRO_BLOCK                       \
            const auto index = handle.index();                                                                          \
            BEE_ASSERT_F(index < capacity, "HandleTable: invalid handle index: out of range of the tables capacity");   \
            BEE_ASSERT_F(indices_[index].dense_index != IndexData::invalid_index, "HandleTable: handle references destroyed data");                          \
            BEE_ASSERT_F(indices_[index].version == handle.version(), "HandleTable: handle version is out of date with allocated version");   \
    BEE_END_MACRO_BLOCK


template <u64 Capacity, typename HandleType, typename DataType>
HandleTable<Capacity, HandleType, DataType>::HandleTable() noexcept
    : next_available_index_(0),
      size_(0)

{
    reset();
}

template <u64 Capacity, typename HandleType, typename DataType>
HandleTable<Capacity, HandleType, DataType>::HandleTable(HandleTable<Capacity, HandleType, DataType>&& other) noexcept
{
    move_construct(BEE_FORWARD(other));
}

template <u64 Capacity, typename HandleType, typename DataType>
HandleTable<Capacity, HandleType, DataType>& HandleTable<Capacity, HandleType, DataType>::operator=(HandleTable<Capacity, HandleType, DataType>&& other) noexcept
{
    move_construct(BEE_FORWARD(other));
    return *this;
}

template <u64 Capacity, typename HandleType, typename DataType>
void HandleTable<Capacity, HandleType, DataType>::move_construct(HandleTable<Capacity, HandleType, DataType>&& other) noexcept
{
    next_available_index_ = other.next_available_index_;
    size_ = other.size_;
    memcpy(indices_, other.indices_, capacity);
    memcpy(dense_to_sparse_, other.dense_to_sparse_, capacity);
    for (int data_idx = 0; data_idx < capacity; ++data_idx)
    {
        data_[data_idx] = BEE_MOVE(other.data_[data_idx]);
    }

    other.reset();
}

template <u64 Capacity, typename HandleType, typename DataType>
HandleType HandleTable<Capacity, HandleType, DataType>::create_uninitialized(DataType** new_data)
{
    BEE_ASSERT(new_data != nullptr);

    // initialize the pointer to nullptr in case of failure
    *new_data = nullptr;

    if (BEE_FAIL_F(size_ < capacity, "HandleTable: reached capacity"))
    {
        return HandleType();
    }

    /*
     * Get the dense index from the sparse, i.e. from:
     *
     *  Sparse: |  0  | uint_max | uint_max |  1  | uint_max | ...
     *             |       ____________________|
     *             |       |
     *             V       V
     *  Dense:  | data | data | - | - | - | ...
     *
     */
    const auto sparse_index = next_available_index_;
    auto& index_data = indices_[sparse_index];

    // Get the next free sparse index for pooling the data mapping sparse=>dense
    next_available_index_ = index_data.next_dense_index;

    // Allocate the dense index - store this on the data itself so we can map in the reverse direction from dense=>sparse
    index_data.dense_index = size_++;
    ++index_data.version;

    // Get the new data and add the mapping in the reverse direction (dense=>sparse)
    *new_data = &data_[index_data.dense_index];
    dense_to_sparse_[index_data.dense_index] = sparse_index;

    return HandleType(sparse_index, index_data.version);
}

template <u64 Capacity, typename HandleType, typename DataType>
HandleType HandleTable<Capacity, HandleType, DataType>::create(const DataType& value)
{
    DataType* new_data = nullptr;
    const auto handle = create_uninitialized(&new_data);
    new (new_data) DataType(value);
    return handle;
}

template <u64 Capacity, typename HandleType, typename DataType>
template <typename... Args>
HandleType HandleTable<Capacity, HandleType, DataType>::emplace(DataType** new_data, Args&&... args) noexcept
{
    BEE_ASSERT(new_data != nullptr);
    const auto handle = create_uninitialized(new_data);
    new (*new_data) DataType(BEE_FORWARD(args)...);
    return handle;
}

template <u64 Capacity, typename HandleType, typename DataType>
void HandleTable<Capacity, HandleType, DataType>::destroy(const HandleType& handle)
{
    BEE_HANDLE_TABLE_VALIDATE(handle);
    BEE_ASSERT(size_ > 0);

    const auto handle_idx = handle.index();
    const auto last_data_idx = --size_;
    auto& index_data = indices_[handle_idx];
    auto& data = data_[index_data.dense_index];

    data.~DataType();

    // Swap the destroyed data with the last valid data item and update the lookups
    if (last_data_idx > 0)
    {
        std::swap(data_[index_data.dense_index], data_[last_data_idx]);
        dense_to_sparse_[index_data.dense_index] = dense_to_sparse_[last_data_idx];
        indices_[dense_to_sparse_[index_data.dense_index]].dense_index = index_data.dense_index;
    }

    dense_to_sparse_[last_data_idx] = IndexData::invalid_index;
    indices_[handle_idx].dense_index = IndexData::invalid_index;

    // Update the version and next available index in pool
    index_data.next_dense_index = next_available_index_;
    next_available_index_ = handle_idx;
}

template <u64 Capacity, typename HandleType, typename DataType>
bool HandleTable<Capacity, HandleType, DataType>::contains(const HandleType& handle)
{
    const auto index = handle.index();
    const auto version = handle.version();
    return index < capacity && indices_[index].dense_index < size_ && version == indices_[index].version;
}

template <u64 Capacity, typename HandleType, typename DataType>
template <typename Predicate>
HandleType HandleTable<Capacity, HandleType, DataType>::find(Predicate&& pred)
{
    for (int data_idx = 0; data_idx < size_; ++data_idx)
    {
        if (pred(data_[data_idx]))
        {
            return HandleType(dense_to_sparse_[data_idx], indices_[index].version);
        }
    }

    return HandleType();
}

template <u64 Capacity, typename HandleType, typename DataType>
void HandleTable<Capacity, HandleType, DataType>::clear()
{
    for (u32 data_idx = 0; data_idx < size_; ++data_idx)
    {
        data_[data_idx].~DataType();
    }

    reset();
}

template <u64 Capacity, typename HandleType, typename DataType>
void HandleTable<Capacity, HandleType, DataType>::reset()
{
    size_ = 0;
    next_available_index_ = 0;

    for (int i = 0; i < capacity; ++i)
    {
        indices_[i].dense_index = IndexData::invalid_index;
        indices_[i].next_dense_index = i + 1;
        indices_[i].version = 0;
        dense_to_sparse_[i] = IndexData::invalid_index;
    }
}


template <u64 Capacity, typename HandleType, typename DataType>
DataType* HandleTable<Capacity, HandleType, DataType>::get(const HandleType& handle)
{
    BEE_HANDLE_TABLE_VALIDATE(handle);
    return &data_[indices_[handle.index()].dense_index];
}

template <u64 Capacity, typename HandleType, typename DataType>
DataType* HandleTable<Capacity, HandleType, DataType>::operator[](const HandleType& handle)
{
    return get(handle);
}

template <u64 Capacity, typename HandleType, typename DataType>
const DataType* HandleTable<Capacity, HandleType, DataType>::operator[](const HandleType& handle) const
{
    return get(handle);
}


} // namespace bee