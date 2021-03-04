/*
 *  GUID.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/GUID.hpp"
#include "Bee/Core/TypeTraits.hpp"
#include "Bee/Core/IO.hpp"

BEE_PUSH_WARNING
    BEE_DISABLE_PADDING_WARNINGS
    #include <stdio.h>
    #include <charconv>
BEE_POP_WARNING

namespace bee {


void write_dash_if_needed(io::StringStream* dst, const GUIDFormat format)
{
    if (format != GUIDFormat::digits)
    {
        dst->write('-');
    }
}

void write_dash_if_needed(String* dst_str, const GUIDFormat format)
{
    if (format != GUIDFormat::digits)
    {
        dst_str->append('-');
    }
}

BEE_TRANSLATION_TABLE_FUNC(guid_format_length, GUIDFormat, i32, GUIDFormat::unknown,
    32, // digits
    36, // digits_with_hyphen,
    38, // braced_digits_with_hyphen,
    38  // parens_digits_with_hyphen,
)

/*
 ************************************
 *
 * `guid_to_string` implementation
 *
 ************************************
 */
i32 guid_to_string(const GUID& guid, GUIDFormat format, char* dst, i32 dst_buffer_size)
{
    return guid_to_string(guid, format, make_span(dst, dst_buffer_size));
}

i32 guid_to_string(const GUID& guid, GUIDFormat format, const Span<char>& dst)
{
    if (BEE_FAIL(dst.size() >= guid_format_length(format)))
    {
        return -1;
    }

    if (format == GUIDFormat::unknown)
    {
        return -1;
    }

    io::StringStream writer(dst.data(), dst.size(), 0);

    switch (format)
    {
        case GUIDFormat::braced_digits_with_hyphen:
        {
            writer.write('{');
            break;
        }
        case GUIDFormat::parens_digits_with_hyphen:
        {
            writer.write('(');
            break;
        }
        default: break;
    }

    writer.write_fmt("%08x", *reinterpret_cast<const u32*>(guid.data));
    write_dash_if_needed(&writer, format);

    writer.write_fmt("%04x", *reinterpret_cast<const u16*>(guid.data + 4));
    write_dash_if_needed(&writer, format);

    writer.write_fmt("%04x", *reinterpret_cast<const u16*>(guid.data + 6));
    write_dash_if_needed(&writer, format);

    writer.write_fmt("%04x", *reinterpret_cast<const u16*>(guid.data + 8));
    write_dash_if_needed(&writer, format);

    u64 last_group = 0;
    memcpy(&last_group, guid.data + 10, 6);
    writer.write_fmt("%012zx", last_group);

    switch (format)
    {
        case GUIDFormat::braced_digits_with_hyphen:
        {
            writer.write('}');
            break;
        }
        case GUIDFormat::parens_digits_with_hyphen:
        {
            writer.write(')');
            break;
        }
        default: break;
    }

    return writer.offset();
}

String guid_to_string(const GUID& guid, const GUIDFormat format, Allocator* allocator)
{
    if (format == GUIDFormat::unknown)
    {
        return String(allocator);
    }

    String result(allocator);

    switch (format)
    {
        case GUIDFormat::braced_digits_with_hyphen:
        {
            result.append('{');
            break;
        }
        case GUIDFormat::parens_digits_with_hyphen:
        {
            result.append('(');
            break;
        }
        default: break;
    }

    str::format(&result, "%08x", *reinterpret_cast<const u32*>(guid.data));
    write_dash_if_needed(&result, format);

    str::format(&result, "%04x", *reinterpret_cast<const u16*>(guid.data + 4));
    write_dash_if_needed(&result, format);

    str::format(&result, "%04x", *reinterpret_cast<const u16*>(guid.data + 6));
    write_dash_if_needed(&result, format);

    str::format(&result, "%04x", *reinterpret_cast<const u16*>(guid.data + 8));
    write_dash_if_needed(&result, format);

    u64 last_group = 0;
    memcpy(&last_group, guid.data + 10, 6);
    str::format(&result, "%012zx", last_group);

    switch (format)
    {
        case GUIDFormat::braced_digits_with_hyphen:
        {
            result.append('}');
            break;
        }
        case GUIDFormat::parens_digits_with_hyphen:
        {
            result.append(')');
            break;
        }
        default: break;
    }

    return result;
}

const char* format_guid(const GUID& guid, GUIDFormat format)
{
    static thread_local char buffer[38];

    const auto length = guid_to_string(guid, format, buffer, static_array_length(buffer));

    if (length < static_array_length(buffer))
    {
        buffer[length] = '\0';
    }

    return buffer;
}


/*
 ************************************
 *
 * `guid_from_string` implementation
 *
 ************************************
 */

// TODO(Jacob): write proper GUID parser that handles exact-width groups
GUID guid_from_string(const StringView& string)
{
    GUID result{};
    if (BEE_FAIL(string.size() >= 32))
    {
        return result;
    }

    const auto has_brackets = string[0] == '{' || string[0] == '(';
    const auto end_token = string[0] == '{' ? '}' : ')';
    const auto has_valid_bracket_format = !has_brackets || (has_brackets && *(string.end() - 1) == end_token);

    if (BEE_FAIL_F(has_valid_bracket_format, "Invalid GUID string format: %s", string.c_str()))
    {
        return result;
    }

    const auto has_dashes = string.size() > 32;

    static constexpr i32 src_part_char_count[] = { 8, 4, 4, 4, 12 };
    u32* dst_parts[] = {
        reinterpret_cast<u32*>(result.data),
        reinterpret_cast<u32*>(result.data + 4),
        reinterpret_cast<u32*>(result.data + 6),
        reinterpret_cast<u32*>(result.data + 8)
    };

    const char* part_begin = has_brackets ? string.begin() + 1 : string.begin();
    const char* part_end = part_begin + src_part_char_count[0];

    for (int i = 0; i < static_array_length(dst_parts); ++i)
    {
        auto from_chars_result = std::from_chars(part_begin, part_end, *dst_parts[i], 16);

        if (BEE_FAIL_F(from_chars_result.ec == std::errc(), "Invalid GUID string format: %" BEE_PRIsv, BEE_FMT_SV(string)))
        {
            return GUID{};
        }

        if (BEE_FAIL_F(from_chars_result.ptr == part_end, "Invalid GUID string format: %" BEE_PRIsv, BEE_FMT_SV(string)))
        {
            return GUID{};
        }

        part_begin = part_end;
        if (has_dashes && BEE_CHECK(*part_begin == '-'))
        {
            ++part_begin;
        }
        part_end = part_begin + src_part_char_count[i + 1];
    }

    size_t last_group = 0;

    auto from_chars_result = std::from_chars(part_begin, part_end, last_group, 16);

    if (BEE_FAIL_F(from_chars_result.ec == std::errc() && from_chars_result.ptr == part_end, "Invalid GUID string format: %" BEE_PRIsv, BEE_FMT_SV(string)))
    {
        return GUID{};
    }

    memcpy(result.data + 10, &last_group, 6);
    return result;
}




} // namespace bee
