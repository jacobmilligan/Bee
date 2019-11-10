/*
 *  BasicStruct.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "OtherHeader.hpp"


enum BEE_REFLECT() AnEnum
{
    One,
    Two,
    Three = 56
};

enum class BEE_REFLECT() EnumClass : unsigned int
{
    unknown = 0u,
    A       = 1u << 0u,
    B       = 1u << 1u,
    C       = 1u << 2u
};

struct BEE_REFLECT(tag = "Special struct tag") HeaderStruct
{
    BEE_REFLECT()
    unsigned int int_field;

    BEE_REFLECT()
    char char_field;
};

namespace bee {
namespace test_reflection {


enum class MyClassVersions
{

};

class BEE_REFLECT(serialized, version = 1) MyClass
{
public:
    explicit MyClass(const int value)
        : field(value)
    {}

    BEE_REFLECT()
    constexpr int my_function(const int& x, const char* y)
    {
        return 23;
    }
private:
    BEE_REFLECT(version_added = 1)
    mutable int field;

    BEE_DEPRECATE_FIELD(HeaderStruct s, 2, 3);

    BEE_REFLECT(version_added = 4)
    int val;

    const char* name;
};


class BEE_REFLECT() DerivedClass final : public BaseClass
{
    BEE_REFLECT()
    int get_int() override
    {
        return 23;
    }
};


}
}
