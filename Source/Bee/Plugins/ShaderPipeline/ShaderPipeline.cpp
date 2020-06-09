/*
 *  ShaderPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/Plugins/AssetPipeline/AssetPipeline.hpp"
#include "Bee/Plugins/AssetRegistry/AssetRegistry.hpp"


namespace bee {


void load_shader_compiler(PluginRegistry* registry, const PluginState state);
void load_shader_loader(PluginRegistry* registry, const PluginState state);
void load_material_compiler(PluginRegistry* registry, const PluginState state);
void load_material_loader(PluginRegistry* registry, const PluginState state);


} // namespace bee


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    if (registry->has_module(BEE_ASSET_PIPELINE_MODULE_NAME))
    {
        bee::load_shader_compiler(registry, state);
        bee::load_material_compiler(registry, state);
    }

    if (registry->has_module(BEE_ASSET_REGISTRY_MODULE_NAME))
    {
        bee::load_shader_loader(registry, state);
        bee::load_material_loader(registry, state);
    }
}