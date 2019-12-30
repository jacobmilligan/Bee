/*
 *  Value.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Noncopyable.hpp"
#include "Bee/Core/Containers/Array.hpp"

namespace bee {
namespace json {


enum class AllocationMode {
    dynamic,
    fixed
};


enum class ValueType : u8 {
    object,
    array,
    string,
    number,
    boolean,
    null,
    unknown
};


struct ValueHandle {
    i32 id { -1 };

    inline constexpr bool is_valid() const
    {
        return id >= 0;
    }
};

struct ValueData {
    ValueType   type { ValueType::unknown }; // 1 byte

    union Contents {
        i64         integer_value;
        double      double_value{ limits::max<double>() };
    } contents; // 8 bytes

    inline constexpr bool is_valid() const
    {
        return contents.double_value < limits::max<double>();
    }

    inline constexpr bool has_children() const
    {
        return type == ValueType::object || type == ValueType::array;
    }

    inline double as_number() const
    {
        return contents.double_value;
    }

    inline bool as_boolean() const
    {
        return static_cast<bool>(contents.integer_value);
    }

    inline char* as_string() const
    {
        return reinterpret_cast<char*>(contents.integer_value);
    }

    inline i32 as_size() const
    {
        return sign_cast<i32>(contents.integer_value);
    }
};


class BEE_CORE_API ValueAllocator : public Noncopyable {
public:
    ValueAllocator(AllocationMode allocation_mode, i32 initial_capacity);

    ValueAllocator(ValueAllocator&& other) noexcept;

    ~ValueAllocator() = default;

    ValueAllocator& operator=(ValueAllocator&& other) noexcept;

    ValueHandle allocate(ValueType type, const void* data, i32 size);
    u8* reserve(i32 size);
    const ValueData* get(const ValueHandle& handle) const;
    ValueData* get(const ValueHandle& handle);
//    ValueHandle append(const ValueHandle& root, ValueType type, const void* value, i32 size);

    inline i32 size() const
    {
        return size_;
    }

    inline u8* data()
    {
        return buffer_.data();
    }

    inline const u8* data() const
    {
        return buffer_.data();
    }

    inline void reset()
    {
        size_ = 0;
    }
private:
    AllocationMode  allocation_mode_;
    i32             size_;
    FixedArray<u8>  buffer_;

    bool ensure_capacity(i32 new_capacity);
    const ValueData* get_internal(const ValueHandle& handle) const;
};

inline constexpr i32 get_offset_buffer_element_count(const ValueHandle& array)
{
    return array.id + static_cast<i32>(sizeof(ValueData));
}

inline constexpr i32 get_offset_buffer_begin(const ValueHandle& array)
{
    return array.id + static_cast<i32>(sizeof(ValueData)) + static_cast<i32>(sizeof(i32));
}

/*
 *************************
 *
 * Object iterators
 *
 *************************
 */
class Document;

struct KeyValueIterItem
{
    const char* key { nullptr };
    ValueHandle value;
};

namespace {

template <bool IsConstIterator>
struct ConstHelper;

template <>
struct ConstHelper<true>
{
    using allocator_t = const ValueAllocator;
};

template <>
struct ConstHelper<false>
{
    using allocator_t = ValueAllocator;
};

}

template <bool IsConst>
class MaybeConstObjectIterator
{
private:
    static constexpr i32 value_size_ = static_cast<i32>(sizeof(ValueData));
public:
    using value_type = KeyValueIterItem;
    using pointer = value_type*;
    using reference = value_type&;
    using difference_type = i32;
    using allocator_t = typename ConstHelper<IsConst>::allocator_t;

    MaybeConstObjectIterator(allocator_t* allocator, const ValueHandle& root)
        : allocator_(allocator)
    {
        // Skip past object to first child member
        current_member_.value.id = root.id + value_size_;
        move_past_key();
    }

    MaybeConstObjectIterator(const MaybeConstObjectIterator& other)
        : allocator_(other.allocator_),
          current_member_(other.current_member_)
    {}

    MaybeConstObjectIterator& operator=(const MaybeConstObjectIterator& other)
    {
        allocator_ = other.allocator_;
        current_member_ = other.current_member_;
        return *this;
    }

    inline reference operator*()
    {
        return current_member_;
    }

    inline pointer operator->()
    {
        return &current_member_;
    }

    inline MaybeConstObjectIterator& operator++()
    {
        auto value_data = allocator_->get(current_member_.value);

        // Go to next value, skipping children if the member has any
        current_member_.value.id += value_size_;
        if (value_data != nullptr && value_data->has_children())
        {
            current_member_.value.id += value_data->as_size();
        }
        move_past_key();
        return *this;
    }

    inline MaybeConstObjectIterator operator++(int)
    {
        auto new_iter = *this;
        ++*this;
        return new_iter;
    }

    inline constexpr bool operator==(const MaybeConstObjectIterator& other)
    {
        return allocator_ == other.allocator_
            && current_member_.value.id == other.current_member_.value.id;
    }

    inline constexpr bool operator!=(const MaybeConstObjectIterator& other)
    {
        return !(*this == other);
    }

private:
    allocator_t*        allocator_ { nullptr };
    KeyValueIterItem    current_member_;

    inline void move_past_key()
    {
        // Get the key string
        auto key_data = allocator_->get(current_member_.value);
        if (key_data != nullptr)
        {
            current_member_.key = key_data->as_string();
        }

        // Move past key and go to value data
        current_member_.value.id += value_size_;
    }
};

template <bool IsConst>
class ObjectRangeAdapter
{
public:
    using iterator_t = MaybeConstObjectIterator<IsConst>;
    using allocator_t = typename iterator_t::allocator_t;

    ObjectRangeAdapter(allocator_t* allocator, const ValueHandle& root)
        : allocator_(allocator),
          root_(root),
          end_({root.id + allocator->get(root)->as_size()})
    {}

    iterator_t begin()
    {
        return iterator_t(allocator_, root_);
    }

    iterator_t end()
    {
        return iterator_t(allocator_, end_);
    }

private:
    allocator_t*    allocator_ { nullptr };
    ValueHandle     root_;
    ValueHandle     end_;
};

using object_iterator = MaybeConstObjectIterator<false>;
using const_object_iterator = MaybeConstObjectIterator<true>;

using object_range_t = ObjectRangeAdapter<false>;
using const_object_range_t = ObjectRangeAdapter<true>;

/*
 *************************
 *
 * Array iterators
 *
 *************************
 */
class Document;

class ArrayIterator
{
private:
    static constexpr i32 value_size_ = static_cast<i32>(sizeof(ValueData));
public:
    using value_type = ValueHandle;
    using pointer = value_type*;
    using reference = value_type&;
    using difference_type = i32;

    ArrayIterator(const ValueAllocator* allocator, const ValueHandle& array, const i32 index)
        : array_base_(array.id),
          current_offset_(reinterpret_cast<const i32*>(allocator->data() + get_offset_buffer_begin(array)) + index)
    {}

    ArrayIterator(const ArrayIterator& other)
        : current_offset_(other.current_offset_)
    {}

    ArrayIterator& operator=(const ArrayIterator& other)
    {
        current_offset_ = other.current_offset_;
        return *this;
    }

    inline value_type operator*()
    {
        return { array_base_ + *current_offset_ };
    }

    inline ArrayIterator& operator++()
    {
        ++current_offset_;
        return *this;
    }

    inline ArrayIterator operator++(int)
    {
        auto new_iter = *this;
        ++*this;
        return new_iter;
    }

    inline constexpr bool operator==(const ArrayIterator& other)
    {
        return current_offset_ == other.current_offset_;
    }

    inline constexpr bool operator!=(const ArrayIterator& other)
    {
        return !(*this == other);
    }

private:
    const i32     array_base_ { 0 };
    const i32*    current_offset_ { nullptr };
};

class ArrayRangeAdapter
{
public:
    ArrayRangeAdapter(const ValueAllocator* allocator, const ValueHandle& root)
        : allocator_(allocator),
          root_{ root }
    {}

    ArrayIterator begin()
    {
        return ArrayIterator(allocator_, root_, 0);
    }

    ArrayIterator end()
    {
        return ArrayIterator(allocator_, root_, element_count());
    }

    inline i32 element_count() const
    {
        return *reinterpret_cast<const i32*>(allocator_->data() + get_offset_buffer_element_count(root_));
    }

private:
    const ValueAllocator*   allocator_ { nullptr };
    ValueHandle             root_;
};


} // namespace json
} // namespace bee
