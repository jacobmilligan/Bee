/*
 *  Material.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Graphics/Shader.hpp"

namespace bee {


struct BEE_REFLECT(serializable, version = 1) Material
{
    GUID            shader_guid;
    i32             pipeline { -1 };

    BEE_REFLECT(ignored)
    Asset<Shader>   shader;

    // TODO(Jacob): shader parameters
};


} // namespace bee