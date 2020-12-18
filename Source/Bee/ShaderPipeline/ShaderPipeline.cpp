/*
 *  ShaderPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"


namespace bee {


extern void load_compiler_module(bee::PluginLoader* loader, const bee::PluginState state);
extern void load_shader_modules(bee::PluginLoader* loader, const bee::PluginState state);


} // namespace bee


BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    bee::load_compiler_module(loader, state);
    bee::load_shader_modules(loader, state);
}

BEE_PLUGIN_VERSION(0, 0, 0)