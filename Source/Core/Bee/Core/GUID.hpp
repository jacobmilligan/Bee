/*
 *  GUID.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Serialization/Serialization.hpp"

#include <string.h>

namespace bee {


enum class GUIDFormat
{
    /**
     * 00000000000000000000000000000000
     */
    digits,

    /**
     * 00000000-0000-0000-0000-000000000000
     */
    digits_with_hyphen,

    /**
     * {00000000-0000-0000-0000-000000000000}
     */
    braced_digits_with_hyphen,

    /**
     * (00000000-0000-0000-0000-000000000000)
     */
    parens_digits_with_hyphen,

    unknown
};


i32 guid_format_length(const GUIDFormat format);

/*
 ************************************************************************************************************
 *
 * GUID - a globally unique identifier in the form AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE. Implemented using
 * the current platforms UUID implementation (i.e. CoCreateGuid() on windows).
 *
 ************************************************************************************************************
 */
struct BEE_REFLECT(serializable, use_builder) GUID
{
    static constexpr size_t sizeof_data = 16 * sizeof(u8);

    u8  data[16];

#if BEE_DEBUG
    BEE_REFLECT(nonserialized)
    char debug_string[33];
#endif // BEE_DEBUG

    inline const u8* begin() const
    {
        return data;
    }

    inline const u8* end() const
    {
        return data + 16;
    }

    inline u8* begin()
    {
        return data;
    }

    inline u8* end()
    {
        return data + 16;
    }
};


inline bool operator==(const GUID& lhs, const GUID& rhs)
{
    return memcmp(lhs.data, rhs.data, 16) == 0;
}

inline bool operator!=(const GUID& lhs, const GUID& rhs)
{
    return !(lhs == rhs);
}

inline bool operator<(const GUID& lhs, const GUID& rhs)
{
    return memcmp(lhs.data, rhs.data, 16) < 0;
}

inline bool operator<=(const GUID& lhs, const GUID& rhs)
{
    return memcmp(lhs.data, rhs.data, 16) <= 0;
}

inline bool operator>(const GUID& lhs, const GUID& rhs)
{
    return memcmp(lhs.data, rhs.data, 16) > 0;
}

inline bool operator>=(const GUID& lhs, const GUID& rhs)
{
    return memcmp(lhs.data, rhs.data, 16) >= 0;
}

template <>
struct Hash<GUID>
{
    inline u32 operator()(const GUID& key) const
    {
        return get_hash(key.data, sizeof(GUID), 0);
    }
};

template <>
BEE_FORCE_INLINE u32 get_hash(const GUID& object)
{
    return Hash<GUID>{}(object);
}


/*
 ******************
 *
 * GUID functions
 *
 ******************
 */


/**
 * Generates a random GUID using the platforms UUID implementation
 */
BEE_CORE_API GUID generate_guid();

/**
 * Converts a GUID to a string representation in the specified format. Strings containing the hexadecimal
 * characters 'a'-'f' are always lowercase (see: https://tools.ietf.org/html/rfc4122 - section 3)
 */
BEE_CORE_API String guid_to_string(const GUID& guid, GUIDFormat format, Allocator* allocator = system_allocator());

BEE_CORE_API i32 guid_to_string(const GUID& guid, GUIDFormat format, const Span<char>& dst);

BEE_CORE_API i32 guid_to_string(const GUID& guid, GUIDFormat format, char* dst, i32 dst_buffer_size);

/**
 * Parses an input string and returns it as a GUID structure. The input string is parsed as case-insensitive, i.e.
 * accepting either 'a' or 'A' as a valid hexadecimal character (see: https://tools.ietf.org/html/rfc4122 - section 3)
 */
BEE_CORE_API GUID guid_from_string(const StringView& string);


inline void serialize_type(SerializationBuilder* builder, GUID* guid)
{
    static constexpr auto guid_as_digits_size = 32;
    static thread_local char string_buffer[guid_as_digits_size + 1];

    if (builder->mode() == SerializerMode::writing)
    {
        guid_to_string(*guid, GUIDFormat::digits, string_buffer, guid_as_digits_size);
    }

    int size = guid_as_digits_size;
    builder->container(SerializedContainerKind::text, &size)
           .text(string_buffer, guid_as_digits_size, guid_as_digits_size + 1);

    BEE_ASSERT(size == guid_as_digits_size);

    if (builder->mode() == SerializerMode::reading)
    {
        *guid = guid_from_string(string_buffer);
#if BEE_DEBUG
        memcpy(guid->debug_string, string_buffer, guid_as_digits_size);
#endif // BEE_DEBUG
    }
}


} // namespace bee
