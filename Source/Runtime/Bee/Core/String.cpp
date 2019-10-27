/*
 *  String.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/String.hpp"
#include "Bee/Core/Math/Math.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <inttypes.h>

#if BEE_COMPILER_MSVC == 1 && _MSC_VER < 1900
    #define BEE_MSVC_REQUIRE_REPLACEMENT_SNPRINTF
#endif // BEE_COMPILER_MSVC == 1


namespace bee {

/*
 * `String` implementation
 */

String::String(Allocator* allocator)
    : allocator_(allocator)
{}

String::String(const i32 count, const char fill_char, Allocator* allocator)
    : allocator_(allocator)
{
    grow(count);

    for (int char_idx = 0; char_idx < size_; ++char_idx)
    {
        data_[char_idx] = fill_char;
    }
}

String::String(const char* c_str, Allocator* allocator)
    : String(StringView(c_str, str::length(c_str)), allocator)
{}

String::String(const StringView& string_view, Allocator* allocator)
    : allocator_(allocator)
{
    grow(string_view.size());

    if (size_ > 0)
    {
        memcpy(data_, string_view.c_str(), string_view.size());
    }
}

String::String(const String& other)
{
    copy_construct(other);
}

String::String(String&& other) noexcept
{
    move_construct(other);
}

String& String::operator=(const String& other)
{
    copy_construct(other);
    return *this;
}

String& String::operator=(String&& other) noexcept
{
    move_construct(other);
    return *this;
}

String& String::operator=(const char* other)
{
    c_string_construct(other, str::length(other), allocator_ != nullptr ? allocator_ : system_allocator());
    return *this;
}

String& String::operator=(const StringView& other)
{
    c_string_construct(other.c_str(), other.size(), allocator_ != nullptr ? allocator_ : system_allocator());
    return *this;
}

String::~String()
{
    destroy();
}

void String::copy_construct(const String& other)
{
    if (this == &other)
    {
        return;
    }

    c_string_construct(other.data_, other.size_, other.allocator_);
}

void String::move_construct(String& other)
{
    if (this == &other)
    {
        return;
    }

    destroy();

    allocator_ = other.allocator_;
    size_ = other.size_;
    capacity_ = other.capacity_;
    data_ = other.data_;

    other.allocator_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

void String::c_string_construct(const char* c_string, const i32 string_length, Allocator* allocator)
{
    destroy();
    allocator_ = allocator;

    if (string_length > 0)
    {
        grow(string_length);
        memcpy(data_, c_string, string_length);
    }
    else
    {
        data_ = const_cast<char*>(&empty_string_);
    }
}

void String::destroy()
{
    // capacity_ will be < small_data_size if no allocations from the allocator have occurred
    if (size_ >= 0 && allocator_ != nullptr && data_ != nullptr && data_ != &empty_string_)
    {
        BEE_FREE(allocator_, data_);
    }

    allocator_ = nullptr;
    size_ = 0;
    capacity_ = 0;
    data_ = const_cast<char*>(&empty_string_);
}

void String::grow(const i32 new_size)
{
    BEE_ASSERT_F(new_size >= 0, "String::grow: `new_size` must be >= 0");

    if (new_size == 0)
    {
        return;
    }

    if (new_size > capacity_ - 1)
    {
        const auto new_capacity = math::max(capacity_ * growth_factor_, new_size + 1);

        char* new_data = nullptr;

        if (data_ == nullptr || data_ == &empty_string_)
        {
            new_data = static_cast<char*>(BEE_MALLOC_ALIGNED(allocator_, sign_cast<size_t>(new_capacity), sizeof(void*)));
        }
        else
        {
            new_data = static_cast<char*>(BEE_REALLOC(allocator_, data_, sign_cast<size_t>(capacity_), sign_cast<size_t>(new_capacity), sizeof(void*)));
        }


        if (BEE_FAIL_F(new_data != nullptr, "Failed to reallocate string data"))
        {
            return;
        }

        data_ = new_data;
        capacity_ = new_capacity;
    }

    BEE_ASSERT(new_size <= capacity_);

    size_ = new_size;
    data_[new_size] = '\0';
}

String& String::append(char character)
{
    const auto old_size = size_;
    grow(size_ + 1);
    data_[old_size] = character;
    return *this;
}

String& String::append(const StringView& string_view)
{
    const auto old_size = size_;
    grow(size_ + string_view.size());
    memcpy(data_ + old_size, string_view.c_str(), string_view.size());
    return *this;
}

String& String::append(const String& other)
{
    return append(other.view());
}

String& String::append(const char* c_str)
{
    return append(StringView(c_str, str::length(c_str)));
}

String& String::insert(const i32 index, const i32 count, const char character)
{
    BEE_ASSERT_F(index >= 0, "String::insert: `index` must be >= 0");
    BEE_ASSERT_F(index <= size_, "String::insert: `index` must be <= size()");

    if (count <= 0)
    {
        return *this;
    }

    const auto old_size = size_;
    const auto new_size = math::max(old_size + count, index + count);
    grow(new_size);

    BEE_ASSERT(index + count <= size_);

    memmove(data_ + index + count, data_ + index, old_size - index);

    for (int char_idx = 0; char_idx < count; ++char_idx)
    {
        data_[index + char_idx] = character;
    }

    return *this;
}

String& String::insert(const i32 index, const char* c_str)
{
    return insert(index, StringView(c_str, str::length(c_str)));
}

String& String::insert(const i32 index, const String& str)
{
    return insert(index, str.view());
}

String& String::insert(i32 index, const StringView& str)
{
    BEE_ASSERT_F(index >= 0, "String::insert: `index` must be >= 0");

    if (str.empty())
    {
        return *this;
    }

    const auto old_size = size_;
    const auto new_size = math::max(old_size + str.size(), index + str.size());
    grow(new_size);

    BEE_ASSERT(index + str.size() <= size_);

    memmove(data_ + index + str.size(), data_ + index, old_size - index);
    memcpy(data_ + index, str.c_str(), str.size());
    return *this;
}

String& String::remove(const i32 index, const i32 count)
{
    BEE_ASSERT_F(index >= 0, "String::destroy: `index` must be >= 0");

    if (count <= 0)
    {
        return *this;
    }

    BEE_ASSERT(index + count <= size_);

    memmove(data_ + index, data_ + index + count, size_ - (index + count));
    size_ -= count;
    data_[size_] = '\0';

    return *this;
}

String& String::remove(const i32 index)
{
    return remove(index, size_ - index);
}

void String::clear()
{
    memset(data_, 0, sizeof(char) * capacity_);
    size_ = 0;
}


/*
 *******************************
 *
 * `StringView` implementation
 *
 *******************************
 */
StringView::StringView(const char* src, const i32 size)
    : data_(src),
      size_(size)
{}

StringView::StringView(const char* src)
    : data_(src),
      size_(str::length(src))
{}

StringView& StringView::operator=(const char* src) noexcept
{
    data_ = src;
    size_ = str::length(src);
    return *this;
}

/*
 **************************************************************
 *
 * `str` utility function namespace implementations
 *
 **************************************************************
 */
namespace str {

#ifdef BEE_MSVC_REQUIRE_REPLACEMENT_SNPRINTF

i32 system_sprintf (char *buffer, const size_t length, const char *format, va_list command_line)
{
    auto length_result = _vsnprintf_s(buffer, length, _TRUNCATE, format, command_line);
    if (length_result == -1)
    {
        length_result = _vscprintf(format, command_line);
    }

    return length_result;
}

#else

i32 system_snprintf(char* buffer, size_t buffer_size, const char* format, va_list args)
{
    return ::vsnprintf(buffer, buffer_size, format, args);
}

#endif // BEE_MSVC_REQUIRE_REPLACEMENT_SNPRINTF

/*
 * `compare` implementation
 */
i32 compare_n(const char* lhs, const i32 lhs_compare_count, const char* rhs, const i32 rhs_compare_count)
{
    int char_idx = 0;
    for (; char_idx < rhs_compare_count; ++char_idx)
    {
        if (char_idx > lhs_compare_count || char_idx > rhs_compare_count)
        {
            break;
        }

        if (lhs[char_idx] != rhs[char_idx] || lhs[char_idx] == '\0' || rhs[char_idx] == '\0')
        {
            break;
        }
    }

    if (char_idx == math::min(lhs_compare_count, rhs_compare_count))
    {
        return lhs_compare_count - rhs_compare_count;
    }

    return static_cast<i32>(lhs[char_idx]) - static_cast<i32>(rhs[char_idx]);
}

i32 compare_n(const char* lhs, const char* rhs, i32 max_compare_count)
{
    return compare_n(lhs, str::length(lhs), rhs, max_compare_count);
}

i32 compare_n(const String& lhs, const char* rhs, i32 max_compare_count)
{
    return compare_n(lhs.c_str(), lhs.size(), rhs, max_compare_count);
}

i32 compare_n(const StringView& lhs, const char* rhs, i32 max_compare_count)
{
    return compare_n(lhs.c_str(), lhs.size(), rhs, max_compare_count);
}

i32 compare(const String& lhs, const String& rhs)
{
    return compare_n(lhs.c_str(), lhs.size(), rhs.c_str(), lhs.size());
}

i32 compare(const String& lhs, const char* rhs)
{
    return compare_n(lhs.c_str(), lhs.size(), rhs, str::length(rhs));
}

i32 compare(const String& lhs, const StringView& rhs)
{
    return compare_n(lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size());
}

i32 compare(const StringView& lhs, const StringView& rhs)
{
    return compare_n(lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size());
}

i32 compare(const StringView& lhs, const char* rhs)
{
    return compare_n(lhs.c_str(), lhs.size(), rhs, str::length(rhs));
}


/*
 * `string_length` implementation
 */
i32 length(const char* string)
{
    /*
     * Rather than implementing a naive strlen - use the platforms implementation. Especially under libc and glibc
     * the platform-specific strlen is the optimal algorithm to use, often employing SIMD optimizations I can't be
     * bothered writing here
     */
    return strlen(string);
}


/*
 * `copy` implementation
 */
void copy(char* dst, i32 dst_size, const char* src, const i32 src_count)
{
    if (src_count <= 0)
    {
        return;
    }

    BEE_ASSERT(dst != nullptr);
    BEE_ASSERT(src != nullptr);

    const auto dst_last_idx = dst_size > 0 ? dst_size - 1 : 0;
    const auto src_max = math::min(src_count, length(src));
    const auto copy_count = math::min(dst_last_idx, src_max); // dst_size minus null required null terminator

    if (copy_count <= 0)
    {
        return;
    }

    memcpy(dst, src, sign_cast<size_t>(copy_count));
    dst[copy_count] = '\0';
}


void copy(char* dst, i32 dst_size, const StringView& src)
{
    copy(dst, dst_size, src.data(), src.size());
}


String v_format(Allocator* allocator, const char* format, va_list args)
{
    const auto length = system_snprintf(nullptr, 0, format, args);
    String result(length, '\0', allocator);
    // include null-terminator
    system_snprintf(result.data(), sign_cast<size_t>(length + 1), format, args);
    return result;
}

/*
 * `format` implementation
 */

String format(Allocator* allocator, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    auto result = v_format(allocator, format, args);
    va_end(args);
    return result;
}

String format(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    auto result = v_format(system_allocator(), format, args);
    va_end(args);
    return result;
}

void format(char* buffer, i32 buffer_size, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    system_snprintf(buffer, buffer_size, format, args);
    va_end(args);
}


/*
 * `last_index_of` implementation
 */
i32 last_index_of_n(const StringView& src, const char* substring, i32 substring_size)
{
    BEE_ASSERT(substring != nullptr);
    BEE_ASSERT(str::length(src.data()) == src.size());

    // Empty string - no matches
    if (src.size() <= 0)
    {
        return -1;
    }

    // size == 1 - can only match first char
    if (src.size() == 1)
    {
        return str::length(substring) == 1 && src[0] == substring[0] ? 0 : -1;
    }

    // case where substring is larger than the src string
    if (substring_size > src.size())
    {
        return -1;
    }

    const auto substr_end = substring + substring_size;
    auto search_iter = src.end();
    const char* src_iter = nullptr;
    const char* substr_iter = nullptr;

    /*
     * Iterate in reverse until we come to a char == to start of the substring, then iterate forward comparing src
     * and substring chars one-by-one
     */
    while (search_iter != src.begin())
    {
        if (*search_iter == *substring)
        {
            substr_iter = substring;
            src_iter = search_iter;

            // Compare chars
            while (*src_iter++ == *substr_iter++)
            {
                if (substr_iter == substr_end)
                {
                    return sign_cast<i32>(search_iter - src.data());
                }
            }
        }

        --search_iter;
    }

    return -1;
}

i32 last_index_of_n(const String& src, const char* substring, const i32 substring_size)
{
    return last_index_of_n(src.view(), substring, substring_size);
}

i32 last_index_of(const StringView& src, char character)
{
    // Empty string - no matches
    if (src.size() <= 0)
    {
        return -1;
    }

    // size == 1 - can only match first char
    if (src.size() == 1)
    {
        return src[0] == character ? 0 : -1;
    }

    for (int char_idx = src.size() - 1; char_idx > 0; --char_idx)
    {
        if (src[char_idx] == character)
        {
            return char_idx;
        }
    }

    return -1;
}

i32 last_index_of(const String& src, char character)
{
    return last_index_of(src.view(), character);
}

i32 last_index_of(const String& src, const char* substring)
{
    return last_index_of_n(src, substring, str::length(substring));
}

i32 last_index_of(const String& src, const String& substring)
{
    return last_index_of_n(src, substring.c_str(), substring.size());
}

i32 last_index_of(const StringView& src, const char* substring)
{
    return last_index_of_n(src, substring, str::length(substring));
}

i32 last_index_of(const StringView& src, const StringView& substring)
{
    return last_index_of_n(src, substring.c_str(), substring.size());
}

i32 last_index_of(const StringView& src, const String& substring)
{
    return last_index_of_n(src, substring.c_str(), substring.size());
}

/*
 * `first_index_of` implementation
 */
i32 first_index_of_n(const StringView& src, const char* substring, const i32 substring_size)
{
    BEE_ASSERT(substring != nullptr);

    // Empty string - no matches
    if (src.empty())
    {
        return -1;
    }

    // size == 1 - can only match first char
    if (src.size() == 1)
    {
        return str::length(substring) == 1 && src[0] == substring[0] ? 0 : -1;
    }

    // case where substring is larger than the src string
    if (substring_size > src.size())
    {
        return -1;
    }

    const auto substr_end = substring + substring_size;
    auto search_iter = src.begin();
    const char* src_iter = nullptr;
    const char* substr_iter = nullptr;

    /*
     * Iterate in forward until we hit the first char in the substring and then check that all subsequent characters
     * match in both strings
     */
    while (search_iter != src.end())
    {
        if (*search_iter == *substring)
        {
            substr_iter = substring;
            src_iter = search_iter;

            // Compare chars
            while (*src_iter++ == *substr_iter++)
            {
                if (substr_iter == substr_end)
                {
                    return sign_cast<i32>(search_iter - src.data());
                }
            }
        }

        ++search_iter;
    }

    return -1;
}

i32 first_index_of_n(const String& src, const char* substring, i32 substring_size)
{
    return first_index_of_n(src.view(), substring, substring_size);
}

i32 first_index_of(const StringView& src, char character)
{
    // Empty string - no matches
    if (src.empty())
    {
        return -1;
    }

    // size == 1 - can only match first char
    if (src.size() == 1)
    {
        return src[0] == character ? 0 : -1;
    }

    for (int char_idx = 0; char_idx < src.size(); ++char_idx)
    {
        if (src[char_idx] == character)
        {
            return char_idx;
        }
    }

    return -1;
}

i32 first_index_of(const String& src, char character)
{
    return first_index_of(src.view(), character);
}

i32 first_index_of(const String& src, const char* substring)
{
    return first_index_of_n(src, substring, str::length(substring));
}

i32 first_index_of(const String& src, const String& substring)
{
    return first_index_of_n(src, substring.c_str(), substring.size());
}

i32 first_index_of(const StringView& src, const char* substring)
{
    return first_index_of_n(src, substring, str::length(substring));
}

i32 first_index_of(const StringView& src, const StringView& substring)
{
    return first_index_of_n(src, substring.c_str(), substring.size());
}

i32 first_index_of(const StringView& src, const String& substring)
{
    return first_index_of_n(src, substring.c_str(), substring.size());
}

/*
 * `replace` implementations
 */
String& replace(String* src, char old_char, char new_char)
{
    // Use some absurdly large number of replacements to simulate replacing all to avoid rewriting replace_n here
    return replace_n(src, old_char, new_char, limits::max<i32>());
}

String& replace(String* src, const char* old_string, const char* new_string)
{
    // Use some absurdly large number of replacements to simulate replacing all to avoid rewriting replace_n here
    return replace_n(src, old_string, new_string, limits::max<i32>());
}

String& replace(String* src, const String& old_string, const char* new_string)
{
    return replace(src, old_string.c_str(), new_string);
}

String& replace(String* src, const char* old_string, const String& new_string)
{
    return replace(src, old_string, new_string.c_str());
}


/*
 * `replace_n` implementation
 */
String& replace_n(String* dst, char old_char, char new_char, i32 count)
{
    BEE_ASSERT(dst != nullptr);

    if (dst->empty() || count <= 0)
    {
        return *dst;
    }

    auto replacements_remaining = count;

    for (auto& cur_char : *dst)
    {
        if (cur_char == old_char)
        {
            cur_char = new_char;
            --replacements_remaining;
        }

        if (replacements_remaining <= 0)
        {
            break;
        }
    }

    return *dst;
}

String& replace_n(String* dst, const char* old_string, const char* new_string, const i32 count)
{
    BEE_ASSERT(dst != nullptr);

    if (dst->empty() || count <= 0)
    {
        return *dst;
    }

    auto replacements_remaining = count;
    auto dst_iter = dst->begin();

    const auto old_string_size = str::length(old_string);
    const auto new_string_size = str::length(new_string);
    const auto removal_size = math::max(1, old_string_size - new_string_size);

    char* dst_substr_ptr = nullptr;
    const char* old_substr_iter = nullptr;
    const char* old_string_end = old_string + old_string_size;

    while (dst_iter < dst->end() && replacements_remaining > 0)
    {
        // Keep searching if we can't find the start of the old string
        if (*dst_iter != *old_string)
        {
            ++dst_iter;
            continue;
        }

        // Found possible match
        dst_substr_ptr = dst_iter;
        old_substr_iter = old_string;

        // Check all characters are the same comparing from dst_iter to old_string_end
        while (*dst_iter++ == *old_substr_iter++)
        {
            if (old_substr_iter == old_string_end)
            {
                /*
                 * Three cases:
                 *
                 * old_string_size > new_string_size:
                 *  copy new string over old string and remove the difference of old and new size
                 *
                 * old_string_size < new_string_size:
                 *  grow string to at least new_string_size with `insert` and then copy new string into old strings idx
                 *
                 * old_string_size == new_string_size:
                 *  call str::copy and just replace the string
                 */
                if (old_string_size > new_string_size)
                {
                    const auto replacement_end = sign_cast<i32>(dst_substr_ptr + new_string_size - dst->begin());
                    memcpy(dst_substr_ptr, new_string, new_string_size);
                    dst->remove(replacement_end, removal_size);
                }
                else if (old_string_size < new_string_size)
                {
                    const auto replacement_range = static_cast<i32>(dst->end() - dst_substr_ptr);
                    const auto replace_ptr_offset = dst_substr_ptr - dst->begin();
                    const auto src_iter_offset = dst_iter - dst->begin();
                    dst->insert(dst->size() - replacement_range, new_string_size - old_string_size, ' ');

                    /*
                     * inserting into the string can reallocate and cause iterators to become invalid - so we need to
                     * fix up the pointers in case this happens
                     */
                    dst_substr_ptr = dst->data() + replace_ptr_offset;
                    dst_iter = dst->data() + src_iter_offset;

                    // Copy over the string replacement
                    memcpy(dst_substr_ptr, new_string, new_string_size);
                }
                else
                {
                    memcpy(dst_substr_ptr, new_string, new_string_size);
                }

                --replacements_remaining;
                break;
            }
        }
    }

    return *dst;
}

String& replace_n(String* dst, const String& old_string, const char* new_string, i32 count)
{
    return replace_n(dst, old_string.c_str(), new_string, count);
}

String& replace_n(String* dst, const char* old_string, const String& new_string, i32 count)
{
    return replace_n(dst, old_string, new_string.c_str(), count);
}


/*
 * `replace_range` implementation
 */

String& replace_range(String* src, i32 index, i32 size, char new_char)
{
    if (src->empty() || size <= 0)
    {
        return *src;
    }

    for (int char_idx = index; char_idx < index + size; ++char_idx)
    {
        if (char_idx > src->size())
        {
            src->append(new_char);
            continue;
        }

        (*src)[char_idx] = new_char;
    }

    return *src;
}

String& replace_range(String* src, const i32 index, const i32 size, const char* new_string, const i32 new_string_size)
{
    if (src->empty() || size <= 0)
    {
        return *src;
    }

    BEE_ASSERT(index + size <= src->size());
    BEE_ASSERT(new_string_size <= str::length(new_string));

    // copy new string over old string and remove the difference of old and new size
    if (new_string_size < size)
    {
        memcpy(src->data() + index, new_string, new_string_size);
        src->remove(index + new_string_size, size - new_string_size);
        return *src;
    }

    // grow string to at least `new_string_size` with `insert` to make space for new string
    if (new_string_size > size)
    {
        src->insert(index + size, new_string_size - size, ' ');
    }

    // sizes are now equal so copy new string into index
    memcpy(src->data() + index, new_string, new_string_size);
    return *src;
}

String& replace_range(String* src, const i32 index, const i32 size, const char* new_string)
{
    return replace_range(src, index, size, new_string, str::length(new_string));
}

String& replace_range(String* src, const i32 index, const i32 size, const String& new_string)
{
    return replace_range(src, index, size, new_string.c_str(), new_string.size());
}

String& replace_range(String* src, const i32 index, const i32 size, const StringView& new_string)
{
    return replace_range(src, index, size, new_string.c_str(), new_string.size());
}


/*
 * `substring` implementation
 */
StringView substring(const String& src, const i32 index, const i32 size)
{
    BEE_ASSERT(index + size <= src.size());
    return StringView(src.data() + index, size);
}

StringView substring(const String& src, const i32 index)
{
    return substring(src, index, src.size() - index);
}

StringView substring(const StringView& src, const i32 index, const i32 size)
{
    BEE_ASSERT(index + size <= src.size());
    return StringView(src.data() + index, size);
}

StringView substring(const StringView& src, const i32 index)
{
    return substring(src, index, src.size() - index);
}


/*
 * String inspection utilities implementation
 */
bool is_space(const char character)
{
    return isspace(character);
}

bool is_digit(const char character)
{
    return isdigit(character);
}


/*
 * Conversion utilities implementation
 */

String to_string(const i32 value, Allocator* allocator)
{
    return format(allocator, "%" PRIi32, value);
}

String to_string(const u32 value, Allocator* allocator)
{
    return format(allocator, "%" PRIu32, value);
}

String to_string(const i64 value, Allocator* allocator)
{
    return format(allocator, "%" PRIi64, value);
}

String to_string(const u64 value, Allocator* allocator)
{
    return format(allocator, "%" PRIu64, value);
}

String to_string(const float value, Allocator* allocator)
{
    return format(allocator, "%f", value);
}

String to_string(const double value, Allocator* allocator)
{
    return format(allocator, "%f", value);
}

/*
 * `trim` implementations
 */
String& trim_start(String* src, char character)
{
    BEE_ASSERT(src != nullptr);

    if (src->empty())
    {
        return *src;
    }

    int occurrences = 0;
    for (auto& c : *src)
    {
        if (c != character)
        {
            break;
        }
        ++occurrences;
    }

    src->remove(0, occurrences);
    return *src;
}

String& trim_end(String* src, char character)
{
    BEE_ASSERT(src != nullptr);

    if (src->empty())
    {
        return *src;
    }

    int trim_index = src->size() - 1;
    for (; trim_index > 0; --trim_index)
    {
        if ((*src)[trim_index] != character)
        {
            break;
        }
    }

    src->remove(trim_index);
    return *src;
}

String& trim(String* src, char character)
{
    trim_start(src, character);
    return trim_end(src, character);
}


} // namespace string
} // namespace bee
