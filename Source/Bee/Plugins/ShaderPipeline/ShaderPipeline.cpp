/*
 *  ShaderPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee.ShaderPipeline.Descriptor.hpp"
#include "Bee/Core/Plugin.hpp"


namespace bee {


void load_compiler(bee::PluginRegistry* registry, const bee::PluginState state);

void load_asset_loader(bee::PluginRegistry* registry, const bee::PluginState state);


} // namespace bee


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    bee::load_compiler(registry, state);
    bee::load_asset_loader(registry, state);
}