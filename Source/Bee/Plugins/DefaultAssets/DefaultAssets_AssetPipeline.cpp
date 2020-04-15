/*
 *  DefaultAssets_AssetPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipeline/AssetPipeline.hpp"
#include "Bee/Plugins/DefaultAssets/ShaderCompiler/Compile.hpp"

namespace bee {

static ShaderCompiler g_shader_compiler;

void register_compilers(AssetCompilerRegistry* registry)
{
    registry->register_compiler(&g_shader_compiler);
}

void unregister_compilers(AssetCompilerRegistry* registry)
{
    registry->unregister_compiler<ShaderCompiler>();
}


} // namespace bee


static bee::AssetPipelineModule g_module;


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry)
{
    g_module.register_compilers = bee::register_compilers;
    g_module.unregister_compilers = bee::unregister_compilers;

    registry->add_interface(BEE_ASSET_PIPELINE_MODULE_NAME, &g_module);
}

BEE_PLUGIN_API void bee_unload_plugin(bee::PluginRegistry* registry)
{
    registry->remove_interface(&g_module);
}