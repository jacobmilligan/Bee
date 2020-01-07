/*
 *  TestAssetTypes.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Config.hpp"

namespace bee {


struct BEE_REFLECT(serializable) Texture
{
    BEE_REFLECT(nonserialized)
    bool loaded { false };
};


} // namespace bee