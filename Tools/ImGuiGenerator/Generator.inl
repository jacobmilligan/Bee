/*
 *  Generator.inl.h
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Core/String.hpp"


namespace bee {


struct BEE_REFLECT(serializable) Definition
{
    StaticString<1024>  cimguiname;
    StaticString<1024>  ov_cimguiname;
    StaticString<256>   args;
    StaticString<64>    stname;

    BEE_REFLECT(optional)
    StaticString<64>    ret;

    BEE_REFLECT(optional)
    bool                constructor { false };

    BEE_REFLECT(optional)
    bool                templated { false };

    BEE_REFLECT(nonserialized)
    StaticString<1024>  plugin_name;

    const StaticString<1024>& get_name() const
    {
        return ov_cimguiname.empty() ? cimguiname : ov_cimguiname;
    }
};


} // namespace bee