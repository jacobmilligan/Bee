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


template <u64 Capacity, typename HandleType, typename DataType>
class HandleTable : public Noncopyable
{
public:
    using table_t                   = HandleTable<Capacity, HandleType, DataType>;
    using handle_t                  = HandleType;
    using data_t                    = DataType;
    using id_t                      = typename HandleType::generator_t::id_t;

    static constexpr id_t capacity   = static_cast<id_t>(Capacity);

    static_assert(sizeof(id_t) <= 8,
        "Bee: HandleTable<Capacity, HandleType, ResourceType>: HandleType must be declared using the BEE_VERSIONED_HANDLE() "
        "macro and be <= 64 bits in size"
    );

    static_assert(
        math::is_power_of_two(capacity),
        "HandleTable: Capacity must be a power of two"
    );

    static_assert(
        capacity < handle_t::generator_t::index_mask - 1,
        "HandleTable: ResourcePool: Capacity must be less than HandleType::generator_t::index_mask - 1"
    );

    HandleTable() noexcept;

    HandleTable(table_t&& other) noexcept;

    HandleTable& operator=(table_t&& other) noexcept;

    handle_t create_uninitialized(DataType** new_data);

    handle_t create(const DataType& value);

    template <typename... Args>
    handle_t emplace(DataType** new_data, Args&&... args) noexcept;

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

    inline id_t size() const
    {
        return size_;
    }
private:
    struct IndexData
    {
        static constexpr auto invalid_index = limits::max<id_t>();
        id_t version { 0 };
        id_t dense_index { invalid_index };
        id_t next_dense_index { 0 };
    };

    id_t            next_available_index_ { 0 };
    id_t            size_ { 0 };
    IndexData       indices_[capacity];
    DataType        data_[capacity];
    id_t            dense_to_sparse_[capacity];

    DataType* get(const handle_t& handle);

    void move_construct(table_t&& other) noexcept;
};


} // namespace bee

#include "Bee/Core/Containers/HandleTable.inl"
