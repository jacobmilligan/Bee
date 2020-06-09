/*
 *  Material.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/HashMap.hpp"

#include "Bee/Plugins/ShaderPipeline/Shader.hpp"
#include "Bee/Plugins/AssetRegistry/AssetRegistry.hpp"

namespace bee {


struct BEE_REFLECT(serializable, version = 1) MaterialFile
{
    String                          shader;
    StaticString<128>               pipeline;
    DynamicHashMap<String, String>  bindings;

    explicit MaterialFile(Allocator* allocator = system_allocator())
        : shader(allocator),
          bindings(allocator)
    {}
};

struct BEE_REFLECT(serializable, version = 1) Material
{
    Asset<Shader>   shader;
    i32             pipeline;
};


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.ShaderPipeline/Material.generated.inl"
#endif // BEE_ENABLE_REFLECTION