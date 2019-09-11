/*
 *  BSC.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/ShaderCompiler/BSC.hpp"

namespace bee {


BSCTarget bsc_target_from_string(const char* string)
{
    if (str::compare(string, "MSL") == 0)
    {
        return BSCTarget::MSL;
    }

    if (str::compare(string, "HLSL") == 0)
    {
        return BSCTarget::HLSL;
    }

    if (str::compare(string, "SPIRV") == 0)
    {
        return BSCTarget::SPIRV;
    }

    return BSCTarget::none;
}


} // namespace bee