/*
 *  IO.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/IO.hpp"
#include "Bee/Core/Math/Math.hpp"

#include <string.h>
#include <errno.h>


namespace bee {
namespace io {

void v_write(String* dst, const char* format, va_list args)
{
    const auto length = str::system_snprintf(nullptr, 0, format, args);
    const auto old_dst_size = dst->size();
    dst->insert(old_dst_size, length, '\0');
    // include null-terminator
    str::system_snprintf(dst->data() + old_dst_size, sign_cast<size_t>(length + 1), format, args);
}

i32 Stream::get_seek_position(const SeekOrigin origin, const i32 stream_size, const i32 current_offset, const i32 new_offset)
{
    switch (origin)
    {
        case SeekOrigin::begin:
        {
            return math::clamp(new_offset, 0, stream_size);
            break;
        }

        case SeekOrigin::current:
        {
            return math::clamp(current_offset + new_offset, 0, stream_size);
            break;
        }

        case SeekOrigin::end:
        {
            return math::clamp(stream_size + new_offset, 0, stream_size);
            break;
        }
    }

    return current_offset;
}


/*
 *****************************************
 *
 * Memory reader/writer implementations
 *
 *****************************************
 */
i32 MemoryStream::read(void* dst_buffer, i32 dst_buffer_size)
{
    if (BEE_FAIL(can_read()))
    {
        return 0;
    }

    if (current_offset_ >= current_stream_size_)
    {
        return 0;
    }

    const auto new_offset = math::min(current_offset_ + dst_buffer_size, capacity_);
    const auto bytes_read = new_offset - current_offset_;

    if (bytes_read <= 0)
    {
        return 0;
    }

    memcpy(dst_buffer, buffer_ + current_offset_, bytes_read);
    current_offset_ = new_offset;
    return bytes_read;
}

i32 MemoryStream::write(const void* src_buffer, const i32 src_buffer_size)
{
    if (BEE_FAIL(can_write()))
    {
        return 0;
    }

    if (current_offset_ >= capacity_)
    {
        return 0;
    }

    auto write_size = src_buffer_size;
    if (current_offset_ + src_buffer_size >= capacity_)
    {
        write_size = capacity_ - current_offset_;
    }

    write_size = math::max(write_size, 0);
    BEE_ASSERT(write_size >= 0);

    if (src_buffer != nullptr)
    {
        if (mode() == Mode::container && container_->size() < current_offset_ + write_size)
        {
            container_->resize(current_offset_ + write_size);
            buffer_ = container_->data(); // fixup pointer to containers internal buffer
        }

        memcpy(buffer_ + current_offset_, src_buffer, write_size);
        current_offset_ += write_size;
        current_stream_size_ = math::max(current_offset_, current_stream_size_);
    }

    return write_size;
}

i32 MemoryStream::seek(const i32 offset, const SeekOrigin origin)
{
    current_offset_ = get_seek_position(origin, current_stream_size_, current_offset_, offset);
    return current_offset_;
}

/*
 *****************************************
 *
 * File reader/writer implementations
 *
 *****************************************
 */
FileStream::FileStream(FILE* src_file, const char* src_file_mode, const bool close_on_destruct)
    : Stream(file_mode_to_stream_mode(src_file_mode)),
      file_(src_file),
      file_mode_(src_file_mode),
      close_on_destruct_(close_on_destruct)
{
    BEE_ASSERT(file_ != nullptr);

    // Get the filesize
    fseek(file_, 0, SEEK_END);
    size_ = ftell(file_);
    fseek(file_, 0, SEEK_SET);
}

FileStream::FileStream(const Path& path, const char* file_mode)
    : FileStream(path.c_str(), file_mode)
{}

FileStream::FileStream(const char* path, const char* file_mode)
    : Stream(Mode::invalid),
      file_(nullptr),
      close_on_destruct_(true)
{
    reopen(path, file_mode);
}

FileStream::~FileStream()
{
    if (close_on_destruct_)
    {
        BEE_ASSERT(file_ != nullptr);
        fclose(file_);
    }
}

void FileStream::reopen(const Path& path, const char* file_mode)
{
    reopen(path.c_str(), file_mode);
}

void FileStream::reopen(const char* path, const char* file_mode)
{
    if (file_ != nullptr && close_on_destruct_)
    {
        fclose(file_);
    }

    file_mode_ = file_mode;
    file_ = fopen(path, file_mode);
    BEE_ASSERT(file_ != nullptr);

    // Get the filesize
    fseek(file_, 0, SEEK_END);
    size_ = ftell(file_);
    fseek(file_, 0, SEEK_SET);

    BEE_ASSERT(ftell(file_) == 0);

    stream_mode = file_mode_to_stream_mode(file_mode);
}

void FileStream::close()
{
    if (file_ != nullptr)
    {
        fclose(file_);
    }
}

i32 FileStream::read(void* dst_buffer, i32 dst_buffer_size)
{
    if (BEE_FAIL(can_read()))
    {
        return 0;
    }

    const auto size_read = sign_cast<i32>(fread(dst_buffer, 1, dst_buffer_size, file_));
    const auto read_error = ferror(file_) != 0;
    const auto bytes_read = ftell(file_);


    BEE_ASSERT(bytes_read <= size_);
    BEE_ASSERT_F(!read_error, "Failed to bytes_read from file with error %s", strerror(errno));

    return size_read;
}

i32 FileStream::write(const void* src_buffer, i32 src_buffer_size)
{
    if (BEE_FAIL(can_write()))
    {
        return 0;
    }

    const auto size_written = sign_cast<i32>(fwrite(src_buffer, 1, src_buffer_size, file_));
    const auto write_error = size_written != src_buffer_size && ferror(file_) != 0;

    BEE_ASSERT_F(!write_error, "Failed to write to file with error code: %d", errno);

    size_ += size_written;
    return size_written;
}

i32 FileStream::write(const StringView& string)
{
    if (BEE_FAIL(can_write()))
    {
        return 0;
    }

    const auto size_written = sign_cast<i32>(fwrite(string.c_str(), 1, string.size(), file_));
    const auto write_error = size_written != string.size() && ferror(file_) != 0;

    BEE_ASSERT_F(!write_error, "Failed to write to file with error code: %d", errno);

    size_ += size_written;
    return size_written;
}

i32 FileStream::write_v(const char* src_fmt_str, va_list src_fmt_args)
{
    if (BEE_FAIL(can_write()))
    {
        return 0;
    }

    const auto size_written = sign_cast<i32>(vfprintf(file_, src_fmt_str, src_fmt_args));
    const auto write_error = ferror(file_) != 0;

    BEE_ASSERT_F(!write_error, "Failed to write to file with error code: %d", errno);

    size_ += size_written;
    return size_written;
}

i32 FileStream::seek(i32 offset, SeekOrigin origin)
{
    auto fseek_offset = offset;
    int fseek_origin = -1;

    switch (origin)
    {
        case SeekOrigin::begin:
        {
            fseek_origin = SEEK_SET;
            break;
        }

        case SeekOrigin::current:
        {
            fseek_origin = SEEK_CUR;
            break;
        }

        case SeekOrigin::end:
        {
            fseek_origin = SEEK_END;
            break;
        }
    }

    const auto seek_success = fseek(file_, static_cast<long>(fseek_offset), fseek_origin) == 0;
    BEE_ASSERT_F(seek_success, "Failed to seek file");
    return fseek_offset;
}

Stream::Mode FileStream::file_mode_to_stream_mode(const char* file_mode)
{
    struct ModeMapping { Mode mode; const char* file_mode; };
    static constexpr ModeMapping mode_mappings[]
    {
        { Mode::read_only, "r" },
        { Mode::read_only, "rb" },
        { Mode::write_only, "w" },
        { Mode::write_only, "wb" },
        { Mode::write_only, "w+" },
        { Mode::write_only, "wb+" },
        { Mode::write_only, "w+b" },
        { Mode::write_only, "a" },
        { Mode::write_only, "ab" },
        { Mode::write_only, "a+" },
        { Mode::write_only, "ab+" },
        { Mode::write_only, "a+b" },
        { Mode::read_write, "r+" },
        { Mode::read_write, "rb+" },
        { Mode::read_write, "r+b" },
    };

    StringView mode_str(file_mode);

    for (auto& mapping : mode_mappings)
    {
        if (mode_str == mapping.file_mode)
        {
            return mapping.mode;
        }
    }

    return Mode::invalid;
}

/*
 *****************************************
 *
 * String reader/writer implementations
 *
 *****************************************
 */
StringStream::StringStream(const char* read_only_string, const i32 string_length)
    : Stream(Mode::read_only)
{
    string.c_string.capacity_ = string_length;
    string.c_string.current_stream_size_ = string_length;
    string.c_string.data.read_only = read_only_string;
}

StringStream::StringStream(char* read_write_string, const i32 string_capacity, const i32 initial_stream_size)
    : Stream(Mode::read_write)
{
    string.c_string.capacity_ = string_capacity;
    string.c_string.current_stream_size_ = initial_stream_size;
    string.c_string.data.read_write = read_write_string;
}

StringStream::StringStream(const StringView& read_only_string)
    : Stream(Mode::read_only)
{
    string.c_string.capacity_ = read_only_string.size();
    string.c_string.current_stream_size_ = read_only_string.size();
    string.c_string.data.read_only = read_only_string.c_str();
}

StringStream::StringStream(String* read_write_string_container)
    : Stream(Mode::container)
{
    string.container = read_write_string_container;
}

i32 StringStream::read(void* dst_buffer, i32 dst_buffer_size)
{
    if (BEE_FAIL(can_read()))
    {
        return 0;
    }

    if (offset() >= size())
    {
        return 0;
    }

    const auto new_offset = math::min(offset() + dst_buffer_size, capacity());
    const auto bytes_read = new_offset - offset();

    if (bytes_read <= 0)
    {
        return 0;
    }

    memcpy(dst_buffer, data(), bytes_read);
    current_offset_ = new_offset;
    return bytes_read;
}

i32 StringStream::read(String* dst_string, const i32 dst_index, const i32 read_count)
{
    const auto total_read_size = math::min(read_count, size() - offset());
    const auto read_end_pos = dst_index + total_read_size;
    if (read_end_pos > dst_string->size())
    {
        dst_string->insert(dst_string->size(), read_end_pos - dst_string->size(), '\0');
    }
    return read(dst_string->data() + dst_index, total_read_size);
}

i32 StringStream::read(String* dst_string)
{
    return read(dst_string, 0, size());
}

i32 StringStream::write(const void* src_buffer, i32 src_buffer_size)
{
    int write_size = 0;

    if (BEE_FAIL(can_write()))
    {
        return write_size;
    }

    if (mode() == Mode::container && offset() + src_buffer_size > string.container->size())
    {
        string.container->insert(offset(), src_buffer_size, '\0');
    }

    if (offset() < capacity())
    {
        write_size = math::min(capacity() - offset(), src_buffer_size);
        memcpy(data(), src_buffer, sizeof(char) * write_size);
        current_offset_ += write_size;

        if (mode() != Mode::container)
        {
            string.c_string.current_stream_size_ = math::max(current_offset_, string.c_string.current_stream_size_);
        }
    }

    return write_size;
}

i32 StringStream::write(const char src)
{
    return write(&src, 1);
}

i32 StringStream::write(const StringView& src)
{
    return write(src.data(), src.size());
}

i32 StringStream::write_v(const char* fmt, va_list args)
{
    if (BEE_FAIL(can_write()))
    {
        return 0;
    }

    if (offset() >= capacity() && mode() != Mode::container)
    {
        return 0;
    }

    const auto length_needed = str::system_snprintf(nullptr, 0, fmt, args);

    if (mode() == Mode::container && offset() + length_needed > string.container->size())
    {
        string.container->insert(offset(), length_needed, '\0');
    }

    const auto write_size = math::min(capacity() - offset(), length_needed);
    str::system_snprintf(data(), capacity(), fmt, args);

    current_offset_ += write_size;

    if (mode() != Mode::container)
    {
        string.c_string.current_stream_size_ = math::max(offset(), string.c_string.current_stream_size_);
    }

    return write_size;
}

i32 StringStream::write_fmt(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    const auto write_size = write_v(format, args);

    va_end(args);

    return write_size;
}

i32 StringStream::seek(i32 offset, const SeekOrigin origin)
{
    current_offset_ = get_seek_position(origin, size(), current_offset_, offset);
    return current_offset_;
}

i32 StringStream::offset() const
{
    return current_offset_;
}

i32 StringStream::size() const
{
    return mode() == Mode::container ? string.container->size() : string.c_string.current_stream_size_;
}

const char* StringStream::c_str_buffer() const
{
    switch (mode())
    {
        case Mode::read_only:
        {
            return string.c_string.data.read_only;
        }
        case Mode::read_write:
        {
            return string.c_string.data.read_write;
        }
        default: break;
    }

    BEE_ASSERT(string.container != nullptr);
    return string.container->data();
}

const char* StringStream::c_str() const
{
    const char* result = c_str_buffer();

#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    if (mode() != Mode::container)
    {
        BEE_ASSERT_F(*(result + size()) == '\0' || *(result + capacity()) == '\0', "StringStream: the source string is not null-terminated - you can call `StringStream::null_terminate` to ensure the source is a valid c-string");
    }
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1

    return result;
}

StringView StringStream::view() const
{
    return StringView(c_str_buffer(), size());
}

char* StringStream::data()
{
    if (mode() == Mode::read_only)
    {
        return const_cast<char*>(string.c_string.data.read_only) + current_offset_;
    }

    if (mode() == Mode::read_write)
    {
        return string.c_string.data.read_write + current_offset_;
    }

    BEE_ASSERT(string.container != nullptr);
    return string.container->data() + current_offset_;
}

void StringStream::null_terminate()
{
    // String containers already handle null termination and read-only strings can't be modified
    if (mode() == Mode::container || mode() == Mode::read_only)
    {
        return;
    }

    const auto read_write_size = size();
    const auto read_write_capacity = capacity();
    const auto null_terminator = read_write_size < read_write_capacity ? read_write_size : read_write_capacity - 1;
    string.c_string.data.read_write[null_terminator] = '\0';
}


} // namespace io
} // namespace bee
