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

struct BEE_REFLECT(serialized_version = 1, tag = "Special struct tag") HeaderStruct
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

class BEE_REFLECT(serializable, test_float = 2.23945f) MyClass
{
public:
    explicit MyClass(const int value)
        : field(value)
    {}

    constexpr int my_function(const int& x, const char* y)
    {
        return 23;
    }
private:
    mutable int field;

//    BEE_D(HeaderStruct s, 2, 3);

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
