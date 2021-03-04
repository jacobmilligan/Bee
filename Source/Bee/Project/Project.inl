/*
 *  Project.inl
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/PluginDescriptor.hpp"


namespace bee {


struct BEE_REFLECT(serializable, version = 1) ProjectDescriptor
{
    String                                      name;
    BEE_REFLECT(nonserialized)
    Path                                        full_path;
    Path                                        cache_root;
    Path                                        source_root;
    DynamicArray<Path>                          asset_roots;
    DynamicArray<PluginDependencyDescriptor>    plugins;
};


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.Project/Project.generated.inl"
#endif // BEE_ENABLE_REFLECTION