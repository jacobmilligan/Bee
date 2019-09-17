/*
 *  IO.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/String.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/Core/Meta.hpp"

#include <type_traits>
#include <stdio.h>

namespace bee {
namespace io {

/*
 *************************
 *
 * Read API
 *
 *************************
 */

/**
 * `read` - reads data from the reader into the Span given, reading up to `source.size()` bytes and returning
 * the total amount of bytes read
 */
template <typename ReaderType>
inline i32 read(ReaderType* reader, Span<u8> dst)
{
    BEE_ASSERT(reader != nullptr);

    if (dst.size() <= 0 || dst.data() == nullptr)
    {
        return 0;
    }

    return reader->read(dst);
}

template <typename ReaderType>
inline i32 read(ReaderType* reader, String* dst)
{
    BEE_ASSERT(dst != nullptr);
    const auto old_size = dst->size();
    dst->append(reinterpret_cast<const char*>(reader->data()));

    auto read_count = dst->size() - old_size;
    reader->seek(read_count, SeekOrigin::current);
    if (*reinterpret_cast<const char*>(reader->data()) == '\0')
    {
        reader->seek(1, SeekOrigin::current);
    }
    return read_count;
}

template <typename ReaderType, typename DstType>
inline i32 read(ReaderType* reader, Span<DstType>& dst)
{
    return read(reader, dst.to_bytes());
}

template <typename ReaderType, typename DstType>
inline i32 read(ReaderType* reader, DstType* dst)
{
    return read(reader, make_span<u8>(reinterpret_cast<u8*>(dst), sizeof(DstType)));
}

/*
 *************************
 *
 * Write API
 *
 *************************
 */

/**
 * Base write function for writing bytes to a destination writer - used by other `write` functions
 */
template <typename WriterType>
inline i32 write(WriterType* dst, const Span<const u8>& data);

/**
 * Specialization for string types
 */
template <typename WriterType>
inline i32 write(WriterType* dst, const String& src)
{
    // Add 1 to get the null-terminated part of the string as well
    const auto size_with_null_term = src.empty() ? 0 : src.size() + 1;
    return write(dst, make_span<const u8>(reinterpret_cast<const u8*>(src.data()), size_with_null_term));
}

/**
 * Casts the source to bytes and writes them to the `dst` writer. Will static_assert if the type doesn't conform to
 * std::is_trivially_copyable
 */
template <typename WriterType, typename SrcType>
inline i32 write(WriterType* dst, const SrcType& src)
{
    static_assert(
        std::is_trivially_copyable<SrcType>::value,
        "write: `SrcType` must be trivially copyable. Consider implementing a specialization of `write` for that type."
    );

    return write(dst, make_span<u8>(reinterpret_cast<const u8*>(&src), sizeof(SrcType)));
}

/**
 * `write_fmt` - writes a printf-like formatted string to the end of the `dst` string
 */
template <typename T>
inline i32 write_fmt(T* dst, const char* format, ...) BEE_PRINTFLIKE(2, 3);

/*
 ****************************************************************************************************
 *
 * # Readers
 *
 * Readers are classes that read from a source stream, i.e. a file or some memory buffer.
 *
 *
 *
 ****************************************************************************************************
 */

enum class SeekOrigin
{
    begin,
    current,
    end
};

/**
 * Manages the reading/writing of data into files, buffers, strings etc.
 */
class BEE_CORE_API Stream
{
public:
    enum class Mode
    {
        invalid,
        read_only,
        write_only,
        read_write,
        container
    };

    explicit Stream(const Mode initial_mode)
        : stream_mode(initial_mode)
    {}

    virtual ~Stream() = default;

    virtual i32 write(const void* dst_buffer, i32 dst_buffer_size)
    {
        // no-op
        return 0;
    }

    virtual i32 read(void* dst_buffer, i32 dst_buffer_size)
    {
        // no-op
        return 0;
    }

    virtual i32 seek(i32 offset, SeekOrigin origin) = 0;

    virtual i32 offset() const = 0;

    virtual i32 size() const = 0;

    inline Mode mode() const
    {
        return stream_mode;
    }

    inline bool can_read() const
    {
        return stream_mode == Mode::read_only || stream_mode == Mode::read_write || stream_mode == Mode::container;
    }

    inline bool can_write() const
    {
        return stream_mode == Mode::write_only || stream_mode == Mode::read_write || stream_mode == Mode::container;
    }
protected:
    Mode stream_mode { Mode::invalid };

    i32 get_seek_position(SeekOrigin origin, i32 stream_size, i32 current_offset, i32 new_offset);
};


/**
 * # MemoryReader
 *
 * Reads data from a source buffer of bytes into other destination buffers
 */
class BEE_CORE_API MemoryStream : public Stream
{
public:
    MemoryStream(const void* read_only_buffer, const i32 buffer_capacity)
        : Stream(Mode::read_only),
          buffer_(const_cast<u8*>(static_cast<const u8*>(read_only_buffer))),
          capacity_(buffer_capacity),
          current_stream_size_(buffer_capacity)
    {}

    MemoryStream(void* read_write_buffer, const i32 buffer_capacity, const i32 initial_size)
        : Stream(Mode::read_write),
          buffer_(static_cast<u8*>(read_write_buffer)),
          capacity_(buffer_capacity),
          current_stream_size_(initial_size)
    {}

    explicit MemoryStream(DynamicArray<u8>* growable_buffer)
        : Stream(Mode::container),
          buffer_(growable_buffer->data()),
          capacity_(limits::max<i32>()),
          current_stream_size_(growable_buffer->size())
    {}

    i32 read(void* dst_buffer, i32 dst_buffer_size) override;

    i32 write(const void* src_buffer, i32 src_buffer_size) override;

    i32 seek(i32 offset, SeekOrigin origin) override;

    inline void set_stream_size(const i32 new_size)
    {
        BEE_ASSERT(new_size >= 0 && new_size <= capacity_);
        current_stream_size_ = new_size;
    }

    inline i32 offset() const override
    {
        return current_offset_;
    }

    inline const void* data() const
    {
        return buffer_ + current_offset_;
    }

    inline void* data()
    {
        return buffer_ + current_offset_;
    }

    inline i32 size() const override
    {
        return current_stream_size_;
    }

    inline i32 capacity() const
    {
        return capacity_;
    }
private:
    i32                 current_offset_ { 0 };
    i32                 capacity_ { 0 };
    i32                 current_stream_size_ { 0 };
    u8*                 buffer_ { nullptr };
    DynamicArray<u8>*   container_ { nullptr };
};


/**
 * # FileReader
 *
 * Reads data from a file into output buffers - can close the file automatically upon destruction if constructed with
 * this option
 */
class BEE_CORE_API FileStream final : public Stream
{
public:
    FileStream(FILE* src_file, const char* src_file_mode, bool close_on_destruct = false);

    FileStream(const Path& path, const char* file_mode);

    FileStream(const char* path, const char* file_mode);

    ~FileStream() override;

    void reopen(const Path& path, const char* file_mode);

    void reopen(const char* path, const char* file_mode);

    void close();

    i32 read(void* dst_buffer, i32 dst_buffer_size) override;

    i32 write(const void* src_buffer, i32 src_buffer_size) override;

    i32 write(const StringView& string);

    i32 write_v(const char* src_fmt_str, va_list src_fmt_args);

    i32 seek(i32 offset, SeekOrigin origin) override;

    inline i32 offset() const override
    {
        return static_cast<i32>(ftell(file_));
    }

    inline i32 size() const override
    {
        return size_;
    }

    Mode file_mode_to_stream_mode(const char* file_mode);
private:
    FILE*           file_ { nullptr };
    bool            close_on_destruct_ { false };
    i32             size_ { 0 };
    const char*     file_mode_;
};


/**
 * # StringWriter
 *
 * Writes data from source strings into either a destination string or a buffer of `char`
 */
class BEE_CORE_API StringStream final : public Stream
{
public:
    StringStream(const char* read_only_string, const i32 string_size);

    StringStream(char* read_write_string, const i32 string_capacity, const i32 initial_stream_size);

    explicit StringStream(const StringView& read_only_string);

    explicit StringStream(String* read_write_string_container);

    i32 read(void* dst_buffer, i32 dst_buffer_size) override;

    i32 read(String* dst_string, i32 dst_index, i32 read_count);

    i32 read(String* dst_string);

    i32 write(const void* src_buffer, i32 src_buffer_size) override;

    i32 write(const char src);

    i32 write(const StringView& src);

    i32 write_v(const char* fmt, va_list args);

    i32 write_fmt(const char* format, ...) BEE_PRINTFLIKE(2, 3);

    i32 seek(i32 offset, SeekOrigin origin) override;

    i32 offset() const override;

    i32 size() const override;

    inline i32 capacity() const
    {
        return mode() == Mode::container ? string.container->capacity() : string.c_string.capacity_;
    };

    const char* c_str() const;

private:
    union StringUnion
    {
        struct CStringData
        {
            i32     capacity_;
            i32     current_stream_size_;

            union CStringUnion
            {
                const char* read_only;
                char*       read_write;
            } data;
        } c_string;

        String*     container;
    } string;

    i32     current_offset_ { 0 };

    char* data();

    void ensure_null_terminated();
};


} // namespace io
} // namespace bee


#include "Bee/Core/IO.inl"
