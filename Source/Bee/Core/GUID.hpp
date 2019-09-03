//
//  GUID.hpp
//  Skyrocket
//
//  --------------------------------------------------------------
//
//  Created by
//  Jacob Milligan on 25/04/2019
//  Copyright (c) 2019 Jacob Milligan. All rights reserved.
//

#pragma once

#include "Bee/Core/NumericTypes.hpp"
#include "Bee/Core/Hash.hpp"
#include "Bee/Core/Serialization/Serialization.hpp"

#include <string.h>

namespace bee {

/*
 ************************************************************************************************************
 *
 * GUID - a globally unique identifier in the form AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE. Implemented using
 * the current platforms UUID implementation (i.e. CoCreateGuid() on windows).
 *
 ************************************************************************************************************
 */
struct GUID
{
    static constexpr size_t sizeof_data = 16 * sizeof(u8);

    u8  data[16];

#if BEE_DEBUG
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


/**
 * Serializes a GUID without also serializing the whole object with version etc. so it just ends up in text formats
 * (such as JSON) as a key-value pair
 */
template <typename SerializerType>
inline void serialize_type(SerializerType* serializer, GUID* guid, const char* name)
{
    static constexpr i32 string_buffer_size = 33;

    BEE_ASSERT(serializer != nullptr);

    // include null-termination
    char string_buffer[string_buffer_size] { 0 };
    auto convert_size = string_buffer_size;

    if (serializer->mode() == SerializerMode::writing)
    {
        convert_size = guid_to_string(*guid, GUIDFormat::digits, make_span(string_buffer, string_buffer_size));
        BEE_ASSERT(convert_size == string_buffer_size - 1);
    }

    serializer->convert_cstr(string_buffer, convert_size, name);

    if (serializer->mode() == SerializerMode::reading)
    {
        *guid = guid_from_string(string_buffer);
#if BEE_DEBUG
        memcpy(guid->debug_string, string_buffer, string_buffer_size * sizeof(char));
#endif // BEE_DEBUG
    }
}

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

/**
 * Generates a random GUID using the platforms UUID implementation
 */
BEE_API GUID generate_guid();

/**
 * Converts a GUID to a string representation in the specified format. Strings containing the hexadecimal
 * characters 'a'-'f' are always lowercase (see: https://tools.ietf.org/html/rfc4122 - section 3)
 */
BEE_API String guid_to_string(const GUID& guid, GUIDFormat format, Allocator* allocator = system_allocator());

BEE_API i32 guid_to_string(const GUID& guid, GUIDFormat format, const Span<char>& dst);

/**
 * Parses an input string and returns it as a GUID structure. The input string is parsed as case-insensitive, i.e.
 * accepting either 'a' or 'A' as a valid hexadecimal character (see: https://tools.ietf.org/html/rfc4122 - section 3)
 */
BEE_API GUID guid_from_string(const StringView& string);


} // namespace bee
