/*
 *  HandleTable.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/Math/Math.hpp"
#include "Bee/Core/Containers/Array.hpp"


namespace bee {


template <i32 Capacity, typename HandleType, typename DataType>
class HandleTable : public Noncopyable
{
public:
    static constexpr i32 capacity   = Capacity;
    using table_t                   = HandleTable<Capacity, HandleType, DataType>;
    using handle_t                  = HandleType;
    using data_t                    = DataType;

    static_assert(
        std::is_base_of_v<VersionedHandle<typename handle_t::tag_t>, handle_t>,
        "HandleTable: HandleType must derive from VersionedHandle, i.e:\nstruct MyHandle : public VersionedHandle<MyHandle>{}"
    );

    static_assert(
        math::is_power_of_two(capacity),
        "HandleTable: Capacity must be a power of two"
    );

    static_assert(
        capacity < handle_t::index_mask - 1,
        "HandleTable: ResourcePool: Capacity must be less than 2^24 - 1"
    );

    HandleTable();

    HandleTable(table_t&& other) noexcept;

    HandleTable& operator=(table_t&& other) noexcept;

    handle_t create_uninitialized(DataType** new_data);

    handle_t create(const DataType& value);

    template <typename... Args>
    handle_t emplace(Args&&... args) noexcept;

    void destroy(const handle_t& handle);

    bool contains(const handle_t& handle);

    template <typename Predicate>
    handle_t find(Predicate&& pred);

    void clear();

    void reset();

    DataType* operator[](const handle_t& handle);

    const DataType* operator[](const handle_t& handle) const;

    inline DataType* begin()
    {
        return data_;
    }

    inline DataType* end()
    {
        return data_ + size_;
    }

    inline i32 size() const
    {
        return sign_cast<i32>(size_);
    }
private:
    struct IndexData
    {
        static constexpr auto invalid_index = limits::max<u32>();
        u32 version { 0 };
        u32 dense_index { invalid_index };
        u32 next_dense_index { 0 };
    };

    u32             next_available_index_ { 0 };
    u32             size_ { 0 };
    IndexData       indices_[capacity];
    DataType        data_[capacity];
    u32             dense_to_sparse_[capacity];

    DataType* get(const handle_t& handle);

    void move_construct(table_t&& other) noexcept;
};


} // namespace bee

#include "Bee/Core/Containers/HandleTable.inl"
