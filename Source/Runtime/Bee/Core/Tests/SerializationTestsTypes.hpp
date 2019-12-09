/*
 *  SerializationTestsTypes.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include <Bee/Core/ReflectionV2.hpp>
#include <Bee/Core/SerializationV2/Serialization.hpp>
#include "../../../../../Tools/BeeReflect/Tests/BasicStruct.hpp"

namespace bee {


struct BEE_REFLECT() ContainersStruct
{
    DynamicArray<int> int_array;
};

struct BEE_REFLECT(serializable, format = packed) PrimitivesStruct
{
    int       intval { -1 };
    u32       uval { 0 };
    char      charval { 0 };
    bool      boolval { false };
    u8        ubyteval { 0 };
    BEE_REFLECT(nonserialized)
    bool      is_valid { false };
    i8        ibyteval { -1 };
    BEE_REFLECT(nonserialized)
    i32       nonserialized_field { -1 };
};

struct BEE_REFLECT(serializable, version = 3, format = table) PrimitivesStructV2
{
    BEE_REFLECT(id = 0, added = 1)
    int       intval { -1 };

    BEE_REFLECT(id = 1, added = 1)
    u32       uval { 0 };

    BEE_REFLECT(id = 2, added = 1)
    char      charval { 0 };

    BEE_REFLECT(id = 3, added = 1)
    bool      boolval { false };

    BEE_REFLECT(id = 4, added = 1, removed = 2)
    u8        ubyteval { 0 };

    BEE_REFLECT(id = 6, added = 1, removed = 3)
    i8        ibyteval { -1 };

    BEE_DEPRECATED(bool is_valid, id = 5, added = 1, removed = 2);

    BEE_REFLECT(id = 8, nonserialized)
    i32       nonserialized_field { -1 };
};

struct BEE_REFLECT(serializable, serializer = bee::serialize_primitives) PrimitivesStructV3
{
    int       intval { -1 };
    u32       uval { 0 };
    char      charval { 0 };
    bool      boolval { false };
    u8        ubyteval { 0 };
    bool      is_valid { false };
    i8        ibyteval { -1 };
    i32       nonserialized_field { -1 };
};


inline bool operator==(const PrimitivesStruct& lhs, const PrimitivesStruct& rhs)
{
    return lhs.intval == rhs.intval
        && lhs.uval == rhs.uval
        && lhs.charval == rhs.charval
        && lhs.boolval == rhs.boolval
        && lhs.ubyteval == rhs.ubyteval
        && lhs.ibyteval == rhs.ibyteval
        && lhs.is_valid == rhs.is_valid
        && lhs.nonserialized_field == rhs.nonserialized_field;
}

inline bool operator==(const PrimitivesStructV2& lhs, const PrimitivesStructV2& rhs)
{
    return lhs.intval == rhs.intval
        && lhs.uval == rhs.uval
        && lhs.charval == rhs.charval
        && lhs.boolval == rhs.boolval
        && lhs.ubyteval == rhs.ubyteval
        && lhs.ibyteval == rhs.ibyteval
        && lhs.nonserialized_field == rhs.nonserialized_field;
}

inline bool operator==(const PrimitivesStructV3& lhs, const PrimitivesStructV3& rhs)
{
    return lhs.intval == rhs.intval
        && lhs.uval == rhs.uval
        && lhs.charval == rhs.charval
        && lhs.boolval == rhs.boolval
        && lhs.ubyteval == rhs.ubyteval
        && lhs.ibyteval == rhs.ibyteval
        && lhs.is_valid == rhs.is_valid
        && lhs.nonserialized_field == rhs.nonserialized_field;
}

inline bool operator!=(const PrimitivesStruct& lhs, const PrimitivesStruct& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const PrimitivesStructV2& lhs, const PrimitivesStructV2& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const PrimitivesStructV3& lhs, const PrimitivesStructV3& rhs)
{
    return !(lhs == rhs);
}

inline void serialize_primitives(SerializationBuilder* builder)
{
    builder->version(1)
        .add(1, &PrimitivesStructV3::boolval)
        .add(1, 2, &PrimitivesStructV3::is_valid)
        .add(1, &PrimitivesStructV3::uval)
        .add(1, &PrimitivesStructV3::charval);
}

inline void serialize_primitives_removed(SerializationBuilder* builder)
{
    builder->version(3)
           .add(1, &PrimitivesStructV3::boolval)
           .add(1, 2, &PrimitivesStructV3::is_valid)
           .remove<u32>(1, 2, 109)
           .add(1, &PrimitivesStructV3::charval);
}



struct BEE_REFLECT(serializable) Id
{
    u32 value { 0 };
};

struct BEE_REFLECT(serializable) Settings
{
    bool is_active { false };

    struct BEE_REFLECT(serializable) NestedType
    {
        Id id_values[5] = { { 0 }, { 1 }, { 2 }, { 3 }, { 4 } };
    };

    NestedType nested;
};

struct BEE_REFLECT(serializable) TestStruct
{
    int value { 0 };

    DynamicArray<int> array;
    Settings settings;

    enum class BEE_REFLECT(serializable) TestEnum
    {
        value_1,
        value_2,
        value_3
    };
};


} // namespace bee
