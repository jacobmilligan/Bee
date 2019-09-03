//
//  SoA.hpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 23/05/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#pragma once

#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/Meta.hpp"

namespace bee {


/*
 **************************************************************************************************
 *
 * # SoA
 *
 * Container that stores data in a 'Structure of Arrays' layout - essentially a tuple of arrays of
 * homogeneous data.
 *
 ***************************************************************************************************
 */

template <typename... Types>
class SoA : public Noncopyable
{
public:
    SoA() = default;

    SoA(const i32 capacity, Allocator* allocator = system_allocator());

    SoA(SoA&& other) noexcept;

    ~SoA();

    SoA& operator=(SoA&& other) noexcept;

    template <i32 Index>
    get_type_at_index_t<Index, Types...>* get();

    template <i32 Index>
    const get_type_at_index_t<Index, Types...>* get() const;

    template <typename ArrayType>
    ArrayType* get();

    template <typename ArrayType>
    const ArrayType* get() const;

    void push_back(const Types&... values);

    void push_back_no_construct();

    void pop_back();

    void pop_back_no_destruct();

    void clear();

    inline i32 size() const
    {
        return size_;
    }

    inline bool empty() const
    {
        return size_ <= 0;
    }

    inline i32 capacity() const
    {
        return capacity_;
    }

    inline constexpr i32 type_count() const
    {
        return type_count_;
    }

    inline const u8* data() const
    {
        return data_;
    }
private:
    static constexpr i32 type_count_ = sizeof... (Types);
    static constexpr i32 sizeof_element_ = sizeof_total_v<Types...>;

    Allocator*  allocator_ { nullptr };
    i32         capacity_ { 0 };
    i32         size_ { 0 };
    u8*         data_ { nullptr };
    u8*         array_ptrs_[type_count_];

    void destruct_range(i32 offset, i32 length);

    void move_construct(SoA<Types...>&& other) noexcept;
};


} // namespace bee

#include "Bee/Core/Containers/SoA.inl"