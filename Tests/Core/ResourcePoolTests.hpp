/*
 *  ResourcePoolTests.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"

struct BEE_REFLECT(serializable) MockResource
{
    static constexpr int new_intval = -1;
    static constexpr char new_charval = '\0';
    static constexpr int deallocated_intval = -99;
    static constexpr char deallocated_charval = 'x';

    int intval { new_intval };
    char charval { new_charval };

    ~MockResource()
    {
        intval = deallocated_intval;
        charval = deallocated_charval;
    }
};

inline bool operator==(const MockResource& lhs, const MockResource& rhs)
{
    return lhs.intval == rhs.intval && lhs.charval == rhs.charval;
}