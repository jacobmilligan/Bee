/*
 *  ImGui_AssetPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipeline/AssetPipeline.hpp"
#include "Bee/Core/Filesystem.hpp"

namespace bee {


void import_assets(AssetPipeline* pipeline)
{
    const auto shader_path = Path(BEE_IMGUI_ASSETS_ROOT).join("ImGui.bsc");
    pipeline->import_asset(shader_path, "ImGui/Shaders", "shaders::ImGui");
}

void delete_assets(AssetPipeline* pipeline)
{
    pipeline->delete_asset("shaders::ImGui");
}


} // namespace bee


static bee::AssetPipelineModule asset_module{};


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry)
{
    // Asset pipeline module
    asset_module.import_assets = bee::import_assets;
    asset_module.delete_assets = bee::delete_assets;
    registry->add_interface(BEE_ASSET_PIPELINE_MODULE_NAME, &asset_module);
}

BEE_PLUGIN_API void bee_unload_plugin(bee::PluginRegistry* registry)
{
    registry->remove_interface(&asset_module);
}