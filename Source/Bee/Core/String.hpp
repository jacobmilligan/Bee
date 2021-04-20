/*
 *  String.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Containers/StaticArray.hpp"
#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Span.hpp"

#include <stdarg.h>


namespace bee {


class String;
class StringView;


namespace str {


BEE_CORE_API i32 system_snprintf(char* buffer, size_t buffer_size, const char* format, va_list args);

/**
 * `compare` - compares two strings and returns a value determining their equality:
 *   * if lhs < rhs then the return value is < 0
 *   * if lhs > rhs then the return value is > 0
 *   * if lhs == rhs then the return value is == 0
 */
BEE_CORE_API i32 compare_n(const char* lhs, i32 lhs_compare_count, const char* rhs, i32 rhs_compare_count);

BEE_CORE_API i32 compare_n(const char* lhs, const char* rhs, i32 max_compare_count);

BEE_CORE_API i32 compare_n(const String& lhs, const char* rhs, i32 max_compare_count);

BEE_CORE_API i32 compare_n(const StringView& lhs, const char* rhs, i32 max_compare_count);

BEE_CORE_API i32 compare(const String& lhs, const String& rhs);

BEE_CORE_API i32 compare(const String& lhs, const char* rhs);

BEE_CORE_API i32 compare(const String& lhs, const StringView& rhs);

BEE_CORE_API i32 compare(const StringView& lhs, const StringView& rhs);

BEE_CORE_API i32 compare(const StringView& lhs, const char* rhs);

/**
 * `length` - gets the length of a null-terminated c-string
 */
BEE_CORE_API i32 length(const char* string);


/**
 * `string_copy` - c-string copy function - mostly a wrapper around strncpy
 */
BEE_CORE_API i32 copy(char* dst, i32 dst_size, const char* src, i32 src_count);

BEE_CORE_API i32 copy(char* dst, i32 dst_size, const StringView& src);


} // namespace string


/**
 * `StringView` - similar to Span<const char> - is a constant view into a strings internal data and is very cheap to
 * copy around and reference - able to be used for both `String` objects and raw c-strings statically allocated
*/
class BEE_REFLECT() BEE_CORE_API StringView
{
public:
    StringView() noexcept = default;

    StringView(const char* src, i32 size);

    StringView(const char* src); // NOLINT

    StringView(const char* begin, const char* end);

    StringView(const StringView& other) noexcept = default;

    StringView& operator=(const StringView& other) noexcept = default;

    StringView& operator=(const char* src) noexcept;

    inline const char operator[](const i32 index) const
    {
        BEE_ASSERT(index < size_);
        return data_[index];
    }

    inline const char* data() const
    {
        return data_;
    }

    inline const char* const c_str() const
    {
        return data_;
    }

    inline i32 size() const
    {
        return size_;
    }

    inline bool empty() const
    {
        return size_ <= 0;
    }

    inline const char* begin() const
    {
        return data_;
    }

    inline const char* end() const
    {
        return data_ + size_;
    }

private:
    const char* data_ { nullptr };
    i32         size_ { 0 };
    BEE_PAD(4);
};

#define BEE_TIMESTAMP_FMT "%Y-%m-%d %H:%M:%S"

#define BEE_PRIsv ".*s"

#define BEE_FMT_SV(VIEW) static_cast<int>(VIEW.size()), VIEW.data()


/**
 * String is a wrapper around a raw null-terminated utf-8 char array that owns the array memory and allocator pointer.
 * Provides only a small amount of utility member functions such as `compare` and `append` with all other string
 * operations provided via free functions.
 *
 * The main differences between `String` and `std::string` (aside from being bee::Allocator-friendly) are:
 *
 * ## No Copy-On-Write
 *
 * `String` doesn't use Copy-On-Write semantics - The reasoning behind this is that Bee's allocator model is
 * extremely flexible and the overhead of reference counting and tracking resources is an undesirable and unnecessary
 * optimization if the user already knows up-front how the memory is to be allocated and used. An example of this is
 * temporary strings allocated from a `StackAllocator` - these strings are created from a pre-allocated memory chunk
 * so are very cheap and can be copied around without worrying about overhead - in this case, COW would a large impact
 * on what could be very cheap operations.
 */
class BEE_REFLECT(serializable, use_builder) BEE_CORE_API String
{
public:
    explicit String(Allocator* allocator = system_allocator()) noexcept;

    String(i32 count, char fill_char, Allocator* allocator = system_allocator());

    explicit String(const char* c_str, Allocator* allocator = system_allocator());

    explicit String(const StringView& string_view, Allocator* allocator = system_allocator());

    template <i32 Size>
    String(const char(&buffer)[Size], Allocator* allocator = system_allocator()) // NOLINT
        : String(buffer, allocator)
    {}

    String(const String& other);

    String(String&& other) noexcept;

    String& operator=(const String& other);

    String& operator=(String&& other) noexcept;

    String& operator=(const char* other);

    String& operator=(const StringView& other);

    ~String();

    String& append(char character);

    String& append(const String& other);

    String& append(const char* c_str);

    String& append(const StringView& string_view);

    String& assign(const String& other);

    String& assign(const char* c_str);

    String& assign(const StringView& string_view);

    String& insert(i32 index, i32 count, char character);

    String& insert(i32 index, const char* c_str);

    String& insert(i32 index, const String& str);

    String& insert(i32 index, const StringView& str);

    String& remove(i32 index, i32 count);

    String& remove(i32 index);

    void resize(i32 size);

    void resize(i32 size, char c);

    void clear();

    inline char& operator[](const i32 index)
    {
        BEE_ASSERT(index < size_);
        return data_[index];
    }

    inline const char& operator[](const i32 index) const
    {
        BEE_ASSERT(index < size_);
        return data_[index];
    }

    inline char& back()
    {
        return data_[size_ - 1];
    }

    inline const char& back() const
    {
        return data_[size_ - 1];
    }

    inline const char* c_str() const
    {
        return data_;
    }

    inline char* begin()
    {
        return data_;
    }

    inline const char* begin() const
    {
        return data_;
    }

    inline char* end()
    {
        return data_ + size_;
    }

    inline const char* end() const
    {
        return data_ + size_;
    }

    inline char* data()
    {
        return data_;
    }

    inline const char* data() const
    {
        return data_;
    }

    inline constexpr bool empty() const
    {
        return size_ <= 0;
    }

    inline constexpr i32 size() const
    {
        return size_;
    }

    inline constexpr i32 capacity() const
    {
        return capacity_;
    }

    inline const Allocator* allocator() const
    {
        return allocator_;
    }

    inline Allocator* allocator()
    {
        return allocator_;
    }

    inline StringView view() const
    {
        return StringView(data_, size_);
    }
private:
    static constexpr i32 growth_factor_ = 2;
    static constexpr char empty_string_ = '\0';

    i32         size_ { 0 };
    i32         capacity_ { 0 };
    Allocator*  allocator_ { nullptr };
    char*       data_ { const_cast<char*>(&empty_string_) };

    friend i32 str::compare(const String& lhs, const String& rhs);

    friend i32 str::compare(const String& lhs, const char* rhs);

    void copy_construct(const String& other);

    void move_construct(String& other);

    void c_string_construct(const char* c_string, i32 string_length, Allocator* allocator);

    void destroy();

    void grow(const i32 new_size);
};


template <i32 Capacity>
class BEE_REFLECT(serializable) StaticString
{
public:
    StaticString() noexcept
        : size_(0)
    {
        buffer_[0] = '\0';
    }

    StaticString(const i32 count, char fill_char)
        : size_(count)
    {
        memset(buffer_, static_cast<int>(fill_char), count);
    }

    explicit StaticString(const StringView& string_view)
    {
        append(string_view);
    }

    explicit StaticString(const char* c_str)
    {
        append(c_str);
    }

    template <i32 BufferSize>
    StaticString(const char(&buffer)[BufferSize]) // NOLINT
    {
        append(buffer);
    }

    StaticString(const StaticString<Capacity>& other)
    {
        append(other.view());
    }

    StaticString(StaticString&& other) noexcept
    {
        append(other.view());
        memset(other.buffer_, 0, Capacity * sizeof(char));
        other.set_size(0);
    }

    StaticString& operator=(const StaticString<Capacity>& other)
    {
        if (this != &other)
        {
            set_size(0);
            append(other.view());
        }

        return *this;
    }

    StaticString& operator=(StaticString<Capacity>&& other) noexcept
    {
        set_size(0);
        append(other.view());
        memset(other.buffer_, 0, Capacity * sizeof(char));
        other.set_size(0);
        return *this;
    }

    StaticString& operator=(const char* other)
    {
        set_size(0);
        append(other);
        return *this;
    }

    StaticString& operator=(const StringView& other)
    {
        set_size(0);
        append(other);
        return *this;
    }

    ~StaticString()
    {
        memset(buffer_, 0, sizeof(char) * Capacity);
        set_size(0);
    }

    StaticString& assign(const String& string)
    {
        clear();
        append(string.view());
        return *this;
    }

    StaticString& assign(const char* c_str)
    {
        clear();
        append(c_str);
        return *this;
    }

    StaticString& assign(const StringView& string_view)
    {
        clear();
        append(string_view);
        return *this;
    }

    StaticString& assign(const StaticString& static_string)
    {
        clear();
        append(StringView(static_string.data(), (static_string.size() < size_) ? static_string.size() : size_));
        return *this;
    }


    StaticString& append(char character)
    {
        if (size_ + 1 <= Capacity)
        {
            buffer_[size_] = character;
            set_size(size_ + 1);
        }
        return *this;
    }

    StaticString& append(const StringView& string_view)
    {
        auto new_size = size_ + string_view.size();
        if (new_size >= Capacity)
        {
            new_size = Capacity;
            memcpy(buffer_ + size_, string_view.c_str(), Capacity - size_);
        }
        else
        {
            memcpy(buffer_ + size_, string_view.c_str(), string_view.size());
        }

        set_size(new_size);
        return *this;
    }

    StaticString& append(const String& other)
    {
        return append(other.view());
    }

    StaticString& append(const char* c_str)
    {
        return append(StringView(c_str));
    }

    StaticString& insert(const i32 index, const i32 count, const char character)
    {
        BEE_ASSERT_F(index >= 0, "StaticString::insert: `index` must be >= 0");
        BEE_ASSERT_F(index <= size_, "StaticString::insert: `index` must be <= size()");

        if (count <= 0)
        {
            return *this;
        }

        auto actual_count = count;

        if (index + count <= Capacity)
        {
            memmove(&buffer_[index + actual_count], &buffer_[index], actual_count);
        }
        else
        {
            actual_count = Capacity - index;
        }

        for (int c = 0; c < actual_count; ++c)
        {
            buffer_[c] = character;
        }

        set_size(size_ + actual_count);
        return *this;
    }

    StaticString& insert(const i32 index, const StringView& str)
    {
        BEE_ASSERT_F(index >= 0, "StaticString::insert: `index` must be >= 0");
        BEE_ASSERT_F(index <= size_, "StaticString::insert: `index` must be <= size()");

        if (str.empty())
        {
            return *this;
        }

        auto actual_count = str.size();

        if (index + str.size() <= Capacity)
        {
            memmove(&buffer_[index + actual_count], &buffer_[index], actual_count);
        }
        else
        {
            actual_count = Capacity - index;
        }

        memcpy(&buffer_[index + index], str.c_str(), actual_count);
        set_size(size_ + actual_count);
        return *this;
    }

    StaticString& insert(const i32 index, const char* c_str)
    {
        return insert(index, StringView(c_str));
    }

    StaticString& insert(const i32 index, const String& str)
    {
        return insert(index, str.view());
    }

    StaticString& remove(const i32 index, const i32 count)
    {
        BEE_ASSERT_F(index >= 0, "StaticString::remove: `index` must be >= 0");
        if (count <= 0)
        {
            return *this;
        }
        BEE_ASSERT(index + count <= size_);
        memmove(&buffer_[index], &buffer_[index + count], size_ - (index + count));
        set_size(size_ - count);
        return *this;
    }

    StaticString& remove(const i32 index)
    {
        return remove(index, size_ - index);
    }

    void resize(const i32 size, const char character)
    {
        BEE_ASSERT_F(size <= Capacity, "StaticString::resize: new size must be <= Size (%d <= %d)", size, Capacity);

        if (size == size_)
        {
            return;
        }

        if (size > size_)
        {
            memset(&buffer_[size_], character, size - size_);
        }

        set_size(size);
    }

    void resize(const i32 size)
    {
        resize(size, '\0');
    }

    void clear()
    {
        size_ = 0;
        memset(buffer_, 0, sizeof(char) * Capacity);
    }

    inline char& operator[](const i32 index)
    {
        BEE_ASSERT(index < size_);
        return buffer_[index];
    }

    inline const char& operator[](const i32 index) const
    {
        BEE_ASSERT(index < size_);
        return buffer_[index];
    }

    inline char& back()
    {
        return buffer_[size_ - 1];
    }

    inline const char& back() const
    {
        return buffer_[size_ - 1];
    }

    inline const char* c_str() const
    {
        return buffer_;
    }

    inline char* begin()
    {
        return buffer_;
    }

    inline const char* begin() const
    {
        return buffer_;
    }

    inline char* end()
    {
        return buffer_ + size_;
    }

    inline const char* end() const
    {
        return buffer_ + size_;
    }

    inline char* data()
    {
        return buffer_;
    }

    inline const char* data() const
    {
        return buffer_;
    }

    inline constexpr bool empty() const
    {
        return size_ <= 0;
    }

    inline constexpr i32 size() const
    {
        return size_;
    }

    inline constexpr i32 capacity() const
    {
        return Capacity;
    }

    inline StringView view() const
    {
        return StringView(buffer_);
    }

private:
    i32     size_ { 0 };
    char    buffer_[Capacity];
    BEE_PAD(8 - ((Capacity + sizeof(i32)) % 8));

    void set_size(const i32 new_size)
    {
        size_ = new_size <= Capacity ? new_size : Capacity;

        if (size_ < Capacity)
        {
            buffer_[size_] = '\0';
        }
    }
};


/*
 ******************************************************************************
 *
 * `String` utility namespace (i.e. format, substring, to_string, to_lower...)
 *
 *******************************************************************************
 */
namespace str {

/**
 * `string_format` - format a string with printf-like format characters (similar to snprintf)
 */
BEE_CORE_API String format(Allocator* allocator, const char* format, ...) BEE_PRINTFLIKE(2, 3);

BEE_CORE_API String format(const char* format, ...) BEE_PRINTFLIKE(1, 2);

BEE_CORE_API i32 format(String* string, const char* format, ...) BEE_PRINTFLIKE(2, 3);

BEE_CORE_API i32 format_buffer(char* buffer, i32 buffer_size, const char* format, ...) BEE_PRINTFLIKE(3, 4);

template <i32 Capacity>
inline i32 format_buffer(StaticString<Capacity>* string, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    const auto length = system_snprintf(nullptr, 0, format, args);

    if (length > Capacity)
    {
        return length;
    }

    string->resize(length);
    // include null-terminator
    system_snprintf(string->data(), sign_cast<size_t>(string->size() + 1), format, args);

    va_end(args);
    return length;
}



/**
 * `last_index_of` - finds the last occurence of a character or substring in the `src` string. Returns -1 if not found
 */
BEE_CORE_API i32 last_index_of_n(const String& src, const char* substring, i32 substring_size);

BEE_CORE_API i32 last_index_of_n(const StringView& src, const char* substring, i32 substring_size);


BEE_CORE_API i32 last_index_of(const String& src, char character);

BEE_CORE_API i32 last_index_of(const String& src, const char* substring);

BEE_CORE_API i32 last_index_of(const String& src, const String& substring);

BEE_CORE_API i32 last_index_of(const StringView& src, char character);

BEE_CORE_API i32 last_index_of(const StringView& src, const char* substring);

BEE_CORE_API i32 last_index_of(const StringView& src, const StringView& substring);

BEE_CORE_API i32 last_index_of(const StringView& src, const String& substring);


/**
 * `first_index_of` - finds the first occurence of a character or substring in the `src` string. Returns -1 if not found
 */
BEE_CORE_API i32 first_index_of_n(const String& src, const char* substring, i32 substring_size);

BEE_CORE_API i32 first_index_of_n(const StringView& src, const char* substring, i32 substring_size);


BEE_CORE_API i32 first_index_of(const String& src, char character);

BEE_CORE_API i32 first_index_of(const String& src, const char* substring);

BEE_CORE_API i32 first_index_of(const String& src, const String& substring);

BEE_CORE_API i32 first_index_of(const StringView& src, char character);

BEE_CORE_API i32 first_index_of(const StringView& src, const char* substring);

BEE_CORE_API i32 first_index_of(const StringView& src, const StringView& substring);

BEE_CORE_API i32 first_index_of(const StringView& src, const String& substring);

/**
 * `replace` - replaces all occurrences of `old_char` with `new_char` in string `src`
 */
BEE_CORE_API String& replace(String* src, char old_char, char new_char);

/**
 * `replace` - replaces all occurrences of `old_string` with `new_string` in string `src`
 */
BEE_CORE_API String& replace(String* src, const char* old_string, const char* new_string);

BEE_CORE_API String& replace(String* src, const String& old_string, const char* new_string);

BEE_CORE_API String& replace(String* src, const char* old_string, const String& new_string);

/**
 * `replace_n` - replaces the first n occurrences of `old_char` with `new_char` in string `src`
 */
BEE_CORE_API String& replace_n(String* dst, char old_char, char new_char, i32 count);

/**
 * `replace_n` - replaces the first n occurrences of `old_string` with `new_string` in string `src`
 */
BEE_CORE_API String& replace_n(String* dst, const char* old_string, const char* new_string, i32 count);

BEE_CORE_API String& replace_n(String* dst, const String& old_string, const char* new_string, i32 count);

BEE_CORE_API String& replace_n(String* dst, const char* old_string, const String& new_string, i32 count);

/**
 * `replace_range` - replaces all characters in the range `index` to `index` + `size` with `new_char`
 */
BEE_CORE_API String& replace_range(String* src, i32 index, i32 size, char new_char);

/**
 * `replace_range` - replaces all characters in the range `index` to `index` + `size` with `new_string`
 */
BEE_CORE_API String& replace_range(String* src, i32 index, i32 size, const char* new_string, i32 new_string_size);

BEE_CORE_API String& replace_range(String* src, i32 index, i32 size, const char* new_string);

BEE_CORE_API String& replace_range(String* src, i32 index, i32 size, const String& new_string);

BEE_CORE_API String& replace_range(String* src, i32 index, i32 size, const StringView& new_string);

/**
 * `substring` - Returns a string containing the characters from `src` string in the range `index` to `index` + `size`
 */
BEE_CORE_API StringView substring(const String& src, i32 index, i32 size);

BEE_CORE_API StringView substring(const String& src, i32 index);

BEE_CORE_API StringView substring(const StringView& src, i32 index, i32 size);

BEE_CORE_API StringView substring(const StringView& src, i32 index);

/**
 * String encoding conversion functions
 */

using wchar_array_t = DynamicArray<wchar_t>;

BEE_CORE_API String from_wchar(const wchar_t* wchar_str, Allocator* allocator = system_allocator());

BEE_CORE_API String from_wchar(const wchar_t* wchar_str, const i32 byte_size, Allocator* allocator = system_allocator());

BEE_CORE_API void from_wchar(String* dst, const wchar_t* wchar_str, const i32 byte_size);

BEE_CORE_API i32 from_wchar(char* dst, const i32 dst_size, const wchar_t* wchar_str, const i32 wchar_size);

BEE_CORE_API wchar_array_t to_wchar(const StringView& src, Allocator* allocator = system_allocator());

BEE_CORE_API i32 to_wchar(const StringView& src, wchar_t* buffer, const i32 buffer_size);

template <i32 Size>
inline StaticArray<wchar_t, Size> to_wchar(const StringView& src)
{
    StaticArray<wchar_t, Size> dst;
    dst.size = to_wchar(src, dst.data, dst.capacity);
    return BEE_MOVE(dst);
}

BEE_CORE_API u32 utf32_to_utf8_codepoint(const u32 utf32_codepoint);


/**
 * String inspection utilities
 */
BEE_CORE_API bool is_space(const char character);

BEE_CORE_API bool is_digit(const char character);

BEE_CORE_API bool is_alpha(const char character);

/**
 * Conversion utilities
 */
BEE_CORE_API i32 to_string_buffer(const i32 value, char* buffer, const i32 size);

BEE_CORE_API i32 to_string_buffer(const u32 value, char* buffer, const i32 size);

BEE_CORE_API i32 to_string_buffer(const i64 value, char* buffer, const i32 size);

BEE_CORE_API i32 to_string_buffer(const u64 value, char* buffer, const i32 size);

BEE_CORE_API i32 to_string_buffer(const float value, char* buffer, const i32 size);

BEE_CORE_API i32 to_string_buffer(const double value, char* buffer, const i32 size);

BEE_CORE_API i32 to_string_buffer(const u128& value, char* buffer, const i32 size);

BEE_CORE_API String to_string(const i32 value, Allocator* allocator = system_allocator());

BEE_CORE_API String to_string(const u32 value, Allocator* allocator = system_allocator());

BEE_CORE_API String to_string(const i64 value, Allocator* allocator = system_allocator());

BEE_CORE_API String to_string(const u64 value, Allocator* allocator = system_allocator());

BEE_CORE_API String to_string(const float value, Allocator* allocator = system_allocator());

BEE_CORE_API String to_string(const double value, Allocator* allocator = system_allocator());

BEE_CORE_API String to_string(const u128& value, Allocator* allocator = system_allocator());

template <i32 Size>
inline void to_static_string(const i32 value, StaticString<Size>* str)
{
    const i32 size = to_string_buffer(value, nullptr, 0);
    str->resize(size);
    to_string_buffer(value, str->data(), str->size());
}

template <i32 Size>
inline void to_static_string(const u32 value, StaticString<Size>* str)
{
    const i32 size = to_string_buffer(value, nullptr, 0);
    str->resize(size);
    to_string_buffer(value, str->data(), str->size());
}

template <i32 Size>
inline void to_static_string(const i64 value, StaticString<Size>* str)
{
    const i32 size = to_string_buffer(value, nullptr, 0);
    str->resize(size);
    to_string_buffer(value, str->data(), str->size());
}

template <i32 Size>
inline void to_static_string(const u64 value, StaticString<Size>* str)
{
    const i32 size = to_string_buffer(value, nullptr, 0);
    str->resize(size);
    to_string_buffer(value, str->data(), str->size());
}

template <i32 Size>
inline void to_static_string(const float value, StaticString<Size>* str)
{
    const i32 size = to_string_buffer(value, nullptr, 0);
    str->resize(size);
    to_string_buffer(value, str->data(), str->size());
}

template <i32 Size>
inline void to_static_string(const double value, StaticString<Size>* str)
{
    const i32 size = to_string_buffer(value, nullptr, 0);
    str->resize(size);
    to_string_buffer(value, str->data(), str->size());
}

template <i32 Size>
inline void to_static_string(const u128& value, StaticString<Size>* str)
{
    const i32 size = to_string_buffer(value, nullptr, 0);
    str->resize(size);
    to_string_buffer(value, str->data(), str->size());
}

BEE_CORE_API bool to_i32(const StringView& src, i32* result);

BEE_CORE_API bool to_u32(const StringView& src, u32* result);

BEE_CORE_API bool to_i64(const StringView& src, i64* result);

BEE_CORE_API bool to_u64(const StringView& src, u64* result);

BEE_CORE_API bool to_u128(const StringView& src, u128* result);

BEE_CORE_API bool to_float(const StringView& src, float* result);

BEE_CORE_API bool to_double(const StringView& src, double* result);


/**
 * `trim` functions - for removing all occurrences of a character at the start/end of a string
 */
BEE_CORE_API String& trim_start(String* src, char character);

BEE_CORE_API String& trim_end(String* src, char character);

BEE_CORE_API String& trim(String* src, char character);

/**
 * `split` - split a StringView using the delimiter character into an array of substrings
 */
BEE_CORE_API void split(const StringView& src, DynamicArray<StringView>* dst, const char* delimiter);

BEE_CORE_API i32 split(const StringView& src, StringView* dst_array, const i32 dst_array_capacity, const char* delimiter);

BEE_CORE_API bool is_ascii(const char c);

/**
 * `to_uppercase_ascii` & `to_lowercase_ascii - convert a utf8 character to it's ASCII uppercase/lowercase equivalent.
 * Returns `c` unchanged if it is not an ASCII character
 */
BEE_CORE_API char to_uppercase_ascii(const char c);

BEE_CORE_API char to_lowercase_ascii(const char c);

/**
 * `uppercase_ascii` - converts `src` to it's ASCII uppercase equivalent, leaving any non-ascii characters unchanged
 */
BEE_CORE_API void uppercase_ascii(String* src);

/**
 * `lowercase_ascii` - converts `src` to it's ASCII uppercase equivalent, leaving any non-ascii characters unchanged
 */
BEE_CORE_API void lowercase_ascii(String* src);

template <i32 Size>
void uppercase_ascii(StaticString<Size>* src)
{
    for (int i = 0; i < src->size(); ++i)
    {
        (*src)[i] = to_uppercase_ascii((*src)[i]);
    }
}


template <i32 Size>
void lowercase_ascii(StaticString<Size>* src)
{
    for (int i = 0; i < src->size(); ++i)
    {
        (*src)[i] = to_lowercase_ascii((*src)[i]);
    }
}


} // namespace str



/*
 ***************************************
 *
 * `String` global operator overloads
 *
 ***************************************
 */

/*
 * `String` - operator+ and operator+=
 */
template <typename T>
inline String operator+(const String& lhs, const T& rhs)
{
    auto result = lhs;
    result.append(rhs);
    return result;
}

template <typename T>
inline String& operator+=(String& lhs, const T& rhs)
{
    return lhs.append(rhs);
}

/*
 * `String` - operator==
 */
inline bool operator==(const String& lhs, const String& rhs)
{
    return str::compare(lhs, rhs) == 0;
}

inline bool operator==(const String& lhs, const StringView& rhs)
{
    return str::compare(lhs, rhs) == 0;
}

inline bool operator==(const String& lhs, const char* rhs)
{
    return str::compare(lhs, rhs) == 0;
}

/*
 * `String` - operator!=
 */
inline bool operator!=(const String& lhs, const String& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const String& lhs, const StringView& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const String& lhs, const char* rhs)
{
    return !(lhs == rhs);
}

/*
 * `String` - operator<
 */
inline bool operator<(const String& lhs, const String& rhs)
{
    return str::compare(lhs, rhs) < 0;
}

inline bool operator<(const String& lhs, const StringView& rhs)
{
    return str::compare(lhs, rhs) < 0;
}

inline bool operator<(const String& lhs, const char* rhs)
{
    return str::compare(lhs, rhs) < 0;
}

/*
 * `String` - operator>
 */
inline bool operator>(const String& lhs, const String& rhs)
{
    return str::compare(lhs, rhs) > 0;
}

inline bool operator>(const String& lhs, const StringView& rhs)
{
    return str::compare(lhs, rhs) > 0;
}

inline bool operator>(const String& lhs, const char* rhs)
{
    return str::compare(lhs, rhs) > 0;
}

/*
 * `String` - operator<=
 */
inline bool operator<=(const String& lhs, const String& rhs)
{
    return str::compare(lhs, rhs) <= 0;
}

inline bool operator<=(const String& lhs, const StringView& rhs)
{
    return str::compare(lhs, rhs) <= 0;
}

inline bool operator<=(const String& lhs, const char* rhs)
{
    return str::compare(lhs, rhs) <= 0;
}

/*
 * `String` - operator>=
 */
inline bool operator>=(const String& lhs, const String& rhs)
{
    return str::compare(lhs, rhs) >= 0;
}

inline bool operator>=(const String& lhs, const StringView& rhs)
{
    return str::compare(lhs, rhs) >= 0;
}

inline bool operator>=(const String& lhs, const char* rhs)
{
    return str::compare(lhs, rhs) >= 0;
}

/*
 ******************************************
 *
 * `StaticString` global operator overloads
 *
 ******************************************
 */


/*
 * `StaticString` - operator==
 */
template <i32 Size>
inline bool operator==(const StaticString<Size>& lhs, const StaticString<Size>& rhs)
{
    return str::compare(lhs.view(), rhs.view()) == 0;
}

template <i32 Size>
inline bool operator==(const StaticString<Size>& lhs, const String& rhs)
{
    return str::compare_n(lhs.view(), rhs.c_str(), rhs.size()) == 0;
}

template <i32 Size>
inline bool operator==(const StaticString<Size>& lhs, const char* rhs)
{
    return str::compare(lhs.view(), rhs) == 0;
}

/*
 * `StaticString` - operator!=
 */
template <i32 Size>
inline bool operator!=(const StaticString<Size>& lhs, const StaticString<Size>& rhs)
{
    return !(lhs == rhs);
}

template <i32 Size>
inline bool operator!=(const StaticString<Size>& lhs, const String& rhs)
{
    return !(lhs == rhs);
}

template <i32 Size>
inline bool operator!=(const StaticString<Size>& lhs, const char* rhs)
{
    return !(lhs == rhs);
}

/*
 * `StaticString` - operator<
 */
template <i32 Size>
inline bool operator<(const StaticString<Size>& lhs, const StaticString<Size>& rhs)
{
    return str::compare(lhs.view(), rhs.view()) < 0;
}

template <i32 Size>
inline bool operator<(const StaticString<Size>& lhs, const String& rhs)
{
    return str::compare_n(lhs.view(), rhs.c_str(), rhs.size()) < 0;
}

template <i32 Size>
inline bool operator<(const StaticString<Size>& lhs, const char* rhs)
{
    return str::compare(lhs.view(), rhs) < 0;
}

/*
 * `StaticString` - operator>
 */
template <i32 Size>
inline bool operator>(const StaticString<Size>& lhs, const StaticString<Size>& rhs)
{
    return str::compare(lhs.view(), rhs.view()) > 0;
}

template <i32 Size>
inline bool operator>(const StaticString<Size>& lhs, const String& rhs)
{
    return str::compare_n(lhs.view(), rhs.c_str(), rhs.size()) > 0;
}

template <i32 Size>
inline bool operator>(const StaticString<Size>& lhs, const char* rhs)
{
    return str::compare(lhs.view(), rhs) > 0;
}

/*
 * `StaticString` - operator<=
 */
template <i32 Size>
inline bool operator<=(const StaticString<Size>& lhs, const StaticString<Size>& rhs)
{
    return str::compare(lhs.view(), rhs.view()) <= 0;
}

template <i32 Size>
inline bool operator<=(const StaticString<Size>& lhs, const String& rhs)
{
    return str::compare_n(lhs.view(), rhs.c_str(), rhs.size()) <= 0;
}

template <i32 Size>
inline bool operator<=(const StaticString<Size>& lhs, const char* rhs)
{
    return str::compare(lhs.view(), rhs) <= 0;
}

/*
 * `StaticString` - operator>=
 */
template <i32 Size>
inline bool operator>=(const StaticString<Size>& lhs, const StaticString<Size>& rhs)
{
    return str::compare(lhs.view(), rhs.view()) >= 0;
}

template <i32 Size>
inline bool operator>=(const StaticString<Size>& lhs, const String& rhs)
{
    return str::compare_n(lhs.view(), rhs.c_str(), rhs.size()) >= 0;
}

template <i32 Size>
inline bool operator>=(const StaticString<Size>& lhs, const char* rhs)
{
    return str::compare(lhs.view(), rhs) >= 0;
}


/*
 ******************************************
 *
 * `StringView` global operator overloads
 *
 ******************************************
 */


/*
 * `StringView` - operator==
 */
inline bool operator==(const StringView& lhs, const StringView& rhs)
{
    return str::compare(lhs, rhs) == 0;
}

inline bool operator==(const StringView& lhs, const String& rhs)
{
    return str::compare_n(lhs, rhs.c_str(), rhs.size()) == 0;
}

inline bool operator==(const StringView& lhs, const char* rhs)
{
    return str::compare(lhs, rhs) == 0;
}

/*
 * `StringView` - operator!=
 */
inline bool operator!=(const StringView& lhs, const StringView& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const StringView& lhs, const String& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const StringView& lhs, const char* rhs)
{
    return !(lhs == rhs);
}

/*
 * `StringView` - operator<
 */
inline bool operator<(const StringView& lhs, const StringView& rhs)
{
    return str::compare(lhs, rhs) < 0;
}

inline bool operator<(const StringView& lhs, const String& rhs)
{
    return str::compare_n(lhs, rhs.c_str(), rhs.size()) < 0;
}

inline bool operator<(const StringView& lhs, const char* rhs)
{
    return str::compare(lhs, rhs) < 0;
}

/*
 * `StringView` - operator>
 */
inline bool operator>(const StringView& lhs, const StringView& rhs)
{
    return str::compare(lhs, rhs) > 0;
}

inline bool operator>(const StringView& lhs, const String& rhs)
{
    return str::compare_n(lhs, rhs.c_str(), rhs.size()) > 0;
}

inline bool operator>(const StringView& lhs, const char* rhs)
{
    return str::compare(lhs, rhs) > 0;
}

/*
 * `StringView` - operator<=
 */
inline bool operator<=(const StringView& lhs, const StringView& rhs)
{
    return str::compare(lhs, rhs) <= 0;
}

inline bool operator<=(const StringView& lhs, const String& rhs)
{
    return str::compare_n(lhs, rhs.c_str(), rhs.size()) <= 0;
}

inline bool operator<=(const StringView& lhs, const char* rhs)
{
    return str::compare(lhs, rhs) <= 0;
}

/*
 * `StringView` - operator>=
 */
inline bool operator>=(const StringView& lhs, const StringView& rhs)
{
    return str::compare(lhs, rhs) >= 0;
}

inline bool operator>=(const StringView& lhs, const String& rhs)
{
    return str::compare_n(lhs, rhs.c_str(), rhs.size()) >= 0;
}

inline bool operator>=(const StringView& lhs, const char* rhs)
{
    return str::compare(lhs, rhs) >= 0;
}


/*
 *******************************************************
 *
 * null-terminated c-string global operator overloads
 *
 *******************************************************
 */

/*
 * operator==
 */
inline bool operator==(const char* lhs, const StringView& rhs)
{
    return str::compare(StringView(lhs), rhs) == 0;
}

inline bool operator==(const char* lhs, const String& rhs)
{
    return str::compare_n(StringView(lhs), rhs.c_str(), rhs.size()) == 0;
}

template <i32 Size>
inline bool operator==(const char* lhs, const StaticString<Size>& rhs)
{
    return str::compare_n(StringView(lhs), rhs.c_str(), rhs.size()) == 0;
}

/*
 * operator!=
 */
inline bool operator!=(const char* lhs, const StringView& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const char* lhs, const String& rhs)
{
    return !(lhs == rhs);
}

template <i32 Size>
inline bool operator!=(const char* lhs, const StaticString<Size>& rhs)
{
    return !(lhs == rhs);
}

/*
 * operator<
 */
inline bool operator<(const char* lhs, const StringView& rhs)
{
    return str::compare(StringView(lhs), rhs) < 0;
}

inline bool operator<(const char* lhs, const String& rhs)
{
    return str::compare_n(StringView(lhs), rhs.c_str(), rhs.size()) < 0;
}

template <i32 Size>
inline bool operator<(const char* lhs, const StaticString<Size>& rhs)
{
    return str::compare_n(StringView(lhs), rhs.c_str(), rhs.size()) < 0;
}

/*
 * operator>
 */
inline bool operator>(const char* lhs, const StringView& rhs)
{
    return str::compare(StringView(lhs), rhs) > 0;
}

inline bool operator>(const char* lhs, const String& rhs)
{
    return str::compare_n(StringView(lhs), rhs.c_str(), rhs.size()) > 0;
}

template <i32 Size>
inline bool operator>(const char* lhs, const StaticString<Size>& rhs)
{
    return str::compare_n(StringView(lhs), rhs.c_str(), rhs.size()) > 0;
}

/*
 * operator<=
 */
inline bool operator<=(const char* lhs, const StringView& rhs)
{
    return str::compare(StringView(lhs), rhs) <= 0;
}

inline bool operator<=(const char* lhs, const String& rhs)
{
    return str::compare_n(StringView(lhs), rhs.c_str(), rhs.size()) <= 0;
}

template <i32 Size>
inline bool operator<=(const char* lhs, const StaticString<Size>& rhs)
{
    return str::compare_n(StringView(lhs), rhs.c_str(), rhs.size()) <= 0;
}

/*
 * operator>=
 */
inline bool operator>=(const char* lhs, const StringView& rhs)
{
    return str::compare(StringView(lhs), rhs) >= 0;
}

inline bool operator>=(const char* lhs, const String& rhs)
{
    return str::compare_n(StringView(lhs), rhs.c_str(), rhs.size()) >= 0;
}

template <i32 Size>
inline bool operator>=(const char* lhs, const StaticString<Size>& rhs)
{
    return str::compare_n(StringView(lhs), rhs.c_str(), rhs.size()) >= 0;
}


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.Core/ReflectedTemplates/String.generated.inl"
#endif // BEE_ENABLE_REFLECTION