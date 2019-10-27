/*
 *  BasicStruct.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/ReflectionV2.hpp"

struct BEE_ATTRIBUTE Attribute
{

};

class MyClass;


struct BEE_REFLECT(Thing(12)) HeaderStruct
{
    BEE_REFLECT()
    unsigned int int_field;

    BEE_REFLECT()
    char char_field;
};

namespace bee {
namespace test_reflection {


class BEE_REFLECT() MyClass
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
    BEE_REFLECT()
    mutable int field;

    BEE_REFLECT()
    HeaderStruct s;

    const char* name;
};


}
}

