/*
 *  String.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Span.hpp"


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


} // namespace string


/**
 * `StringView` - similar to Span<const char> - is a constant view into a strings internal data and is very cheap to
 * copy around and reference - able to be used for both `String` objects and raw c-strings statically allocated
*/
class BEE_CORE_API StringView
{
public:
    constexpr StringView() noexcept = default;

    StringView(const char* src, i32 size);

    StringView(const char* src); // NOLINT(google-explicit-constructor)

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
};

#define BEE_PRIsv ".*s"

#define BEE_FMT_SV(string_view) string_view.size(), string_view.data()


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
class BEE_CORE_API String
{
public:
    explicit String(Allocator* allocator = system_allocator());

    String(i32 count, char fill_char, Allocator* allocator = system_allocator());

    explicit String(const char* c_str, Allocator* allocator = system_allocator());

    explicit String(const StringView& string_view, Allocator* allocator = system_allocator());

    template <i32 Size>
    String(const char(&buffer)[Size], Allocator* allocator = system_allocator())
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

    String& insert(i32 index, i32 count, char character);

    String& insert(i32 index, const char* c_str);

    String& insert(i32 index, const String& str);

    String& insert(i32 index, const StringView& str);

    String& remove(i32 index, i32 count);

    String& remove(i32 index);

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


/*
 ******************************************************************************
 *
 * `String` utility namespace (i.e. format, substring, to_string, to_lower...)
 *
 *******************************************************************************
 */
namespace str {
/**
 * `length` - gets the length of a null-terminated c-string
 */
BEE_CORE_API i32 length(const char* string);


/**
 * `string_copy` - c-string copy function - mostly a wrapper around strncpy
 */
BEE_CORE_API void copy(char* dst, i32 dst_size, const char* src, i32 src_count);

BEE_CORE_API void copy(char* dst, i32 dst_size, const StringView& src);


/**
 * `string_format` - format a string with printf-like format characters (similar to snprintf)
 */
BEE_CORE_API String format(Allocator* allocator, const char* format, ...) BEE_PRINTFLIKE(2, 3);

BEE_CORE_API String format(const char* format, ...) BEE_PRINTFLIKE(1, 2);

BEE_CORE_API void format(char* buffer, i32 buffer_size, const char* format, ...) BEE_PRINTFLIKE(3, 4);


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

BEE_CORE_API wchar_array_t to_wchar(const StringView& src, Allocator* allocator = system_allocator());

/**
 * String inspection utilities
 */
BEE_CORE_API bool is_space(const char character);

BEE_CORE_API bool is_digit(const char character);

/**
 * Conversion utilities
 */
BEE_CORE_API String to_string(const i32 value, Allocator* allocator = system_allocator());

BEE_CORE_API String to_string(const u32 value, Allocator* allocator = system_allocator());

BEE_CORE_API String to_string(const i64 value, Allocator* allocator = system_allocator());

BEE_CORE_API String to_string(const u64 value, Allocator* allocator = system_allocator());

BEE_CORE_API String to_string(const float value, Allocator* allocator = system_allocator());

BEE_CORE_API String to_string(const double value, Allocator* allocator = system_allocator());

/**
 * `trim` functions - for removing all occurrences of a character at the start/end of a string
 */
BEE_CORE_API String& trim_start(String* src, char character);

BEE_CORE_API String& trim_end(String* src, char character);

BEE_CORE_API String& trim(String* src, char character);


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


} // namespace bee
