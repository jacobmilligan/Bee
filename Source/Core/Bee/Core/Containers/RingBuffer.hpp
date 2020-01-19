/*
 *  RingBuffer.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Noncopyable.hpp"
#include "Bee/Core/Memory/Allocator.hpp"
#include "Bee/Core/NumericTypes.hpp"

namespace bee {

// TODO(Jacob): make this biznatch thread safe and lock-free
class BEE_CORE_API RingBuffer : public Noncopyable
{
public:
    RingBuffer() = default;

    explicit RingBuffer(i32 max_size, Allocator* allocator = system_allocator());

    ~RingBuffer();

    RingBuffer(RingBuffer&& other) noexcept;

    RingBuffer& operator=(RingBuffer&& other) noexcept;

    inline i32 max_size() const
    {
        return max_size_;
    }

    inline bool empty() const
    {
        return size_ == 0;
    }

    inline bool full() const
    {
        return size_ == max_size_;
    }

    inline i32 size() const
    {
        return size_;
    }

    inline i32 write_position() const
    {
        return current_write_pos_;
    }

    inline i32 read_position() const
    {
        return current_read_pos_;
    }

    bool write(const void* data, i32 num_bytes_to_write);

    bool read(void* data, i32 num_bytes_to_read);

    bool peek(void* data, i32 num_bytes_to_peek) const;

    void reset();
private:
    i32         max_size_;
    i32         current_read_pos_;
    i32         current_write_pos_;
    i32         size_;
    u8*         data_;
    Allocator*  allocator_;
};


} // namespace bee