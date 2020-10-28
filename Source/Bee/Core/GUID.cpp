/*
 *  GUID.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/GUID.hpp"
#include "Bee/Core/TypeTraits.hpp"
#include "Bee/Core/IO.hpp"

#include <stdio.h>

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

BEE_TRANSLATION_TABLE(guid_format_length, GUIDFormat, i32, GUIDFormat::unknown,
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

    io::write_fmt(&result, "%08x", *reinterpret_cast<const u32*>(guid.data));
    write_dash_if_needed(&result, format);

    io::write_fmt(&result, "%04x", *reinterpret_cast<const u16*>(guid.data + 4));
    write_dash_if_needed(&result, format);

    io::write_fmt(&result, "%04x", *reinterpret_cast<const u16*>(guid.data + 6));
    write_dash_if_needed(&result, format);

    io::write_fmt(&result, "%04x", *reinterpret_cast<const u16*>(guid.data + 8));
    write_dash_if_needed(&result, format);

    u64 last_group = 0;
    memcpy(&last_group, guid.data + 10, 6);
    io::write_fmt(&result, "%012zx", last_group);

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
    const auto guid_begin = has_brackets ? string.begin() + 1 : string.begin();

    u32* scan_data[] = {
        reinterpret_cast<u32*>(result.data),
        reinterpret_cast<u32*>(result.data + 4),
        reinterpret_cast<u32*>(result.data + 6),
        reinterpret_cast<u32*>(result.data + 8)
    };
    size_t last_group = 0;

    int scan_result = 0;
    if (has_dashes)
    {
        scan_result = sscanf(
            guid_begin,
            "%08x-%04x-%04x-%04x-%012zx",
            scan_data[0], scan_data[1], scan_data[2], scan_data[3], &last_group
        );
    }
    else
    {
        scan_result = sscanf(
            guid_begin,
            "%08x%04x%04x%04x%012zx",
            scan_data[0], scan_data[1], scan_data[2], scan_data[3], &last_group
        );
    }

    if (BEE_FAIL_F(scan_result == 5, "Invalid GUID string format: %s", string.c_str()))
    {
        return GUID{};
    }

    memcpy(result.data + 10, &last_group, 6);
    return result;
}




} // namespace bee
