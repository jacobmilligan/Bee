//
//  Value.inl
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 23/04/2019
//  Copyright (c) 2016 Jacob Milligan. All rights reserved.
//

#pragma once

namespace bee {
namespace json {

template <bool IsConst>
MaybeConstObjectIterator<IsConst>::MaybeConstObjectIterator(allocator_t* allocator, const ValueHandle& root)
    : allocator_(allocator)
{
    auto root_data = allocator->get(root);
    root_size_ = root_data != nullptr && root_data->has_children() ? root_data->as_size() : 0;
    set_key_and_value(root.id + value_size_);
}

template <bool IsConst>
MaybeConstObjectIterator<IsConst>::MaybeConstObjectIterator(const MaybeConstObjectIterator& other)
    : allocator_(other.allocator_),
      root_size_(other.root_size_),
      current_member_(other.current_member_)
{}

template <bool IsConst>
MaybeConstObjectIterator<IsConst>& MaybeConstObjectIterator<IsConst>::operator=(const MaybeConstObjectIterator& other)
{
    allocator_ = other.allocator_;
    root_size_ = other.root_size_;
    current_member_ = other.current_member_;
    return *this;
}

template <bool IsConst>
inline typename MaybeConstObjectIterator<IsConst>::reference MaybeConstObjectIterator<IsConst>::operator*()
{
    return current_member_;
}

template <bool IsConst>
inline typename MaybeConstObjectIterator<IsConst>::pointer MaybeConstObjectIterator<IsConst>::operator->()
{
    return &current_member_;
}

template <bool IsConst>
inline MaybeConstObjectIterator<IsConst>& MaybeConstObjectIterator<IsConst>::operator++()
{
    const auto member_data = allocator_->get(current_member_.value);
    const auto next = member_data->has_children() ? member_data->as_size() : value_size_;
    if (next < root_size_)
    {
        set_key_and_value(next);
    }
    return *this;
}

template <bool IsConst>
inline MaybeConstObjectIterator<IsConst> MaybeConstObjectIterator<IsConst>::operator++(int)
{
    auto new_iter = *this;
    ++*this;
    return new_iter;
}

template <bool IsConst>
inline constexpr bool MaybeConstObjectIterator<IsConst>::operator==(const MaybeConstObjectIterator& other)
{
    return allocator_ == other.allocator_
        && root_size_ == other.root_size_
        && current_member_.value.id == other.current_member_.value.id;
}

template <bool IsConst>
inline constexpr bool MaybeConstObjectIterator<IsConst>::operator!=(const MaybeConstObjectIterator& other)
{
    return !(*this == other);
}

template <bool IsConst>
inline void MaybeConstObjectIterator<IsConst>::set_key_and_value(const i32 key_offset)
{
    auto current_member_data = allocator_->get(ValueHandle { key_offset });
    current_member_.key = current_member_data->as_string();
    current_member_.value.id = key_offset + value_size_;
}

template <bool IsConst>
ObjectRangeAdapter<IsConst>::ObjectRangeAdapter(allocator_t* allocator, const ValueHandle& root)
    : allocator_(allocator),
      root_(root)
{
    const auto root_data = allocator_->get(root_);
    end_ = ValueHandle { root_data != nullptr ?  root_data->as_size() : root_.id };
}

template <bool IsConst>
typename ObjectRangeAdapter<IsConst>::iterator_t ObjectRangeAdapter<IsConst>::begin()
{
    return iterator_t(allocator_, root_);
}
template <bool IsConst>
typename ObjectRangeAdapter<IsConst>::iterator_t ObjectRangeAdapter<IsConst>::end()
{
    return iterator_t(allocator_, end_);
}


} // namespace json
} // namespace bee