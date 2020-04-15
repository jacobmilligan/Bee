/*
 *  EditorApp.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/AssetPipeline/AssetPipeline.hpp"
#include "Bee/Core/Plugin.hpp"


namespace bee {


struct BEE_REFLECT(serializable, version = 1) EditorConfig
{
    DynamicArray<Path>  projects;
    i32                 most_recent_project { -1 };
};


struct BEE_REFLECT(serializable, version = 1) Project
{
    StaticString<8>                 engine_version;
    StaticString<256>               name;
    String                          description;
    AssetPlatform                   platform { AssetPlatform::unknown };
    Path                            assets_root;
    Path                            sources_root;
    Path                            cache_root;

    BEE_REFLECT(nonserialized)
    bool                            is_open { false };

    BEE_REFLECT(nonserialized)
    Path                            location;
};


enum class EditorLaunchMode
{
    standard,
    new_project,
    existing_project
};

struct EditorLaunchParameters
{
    EditorLaunchMode    mode { EditorLaunchMode::standard };
    AssetPlatform       platform { default_asset_platform };
    const char*         project_path { nullptr };
    const char*         project_name { nullptr };
};

struct ImGuiApi;

struct EditorContext
{
    EditorLaunchParameters  launch_params;
    EditorConfig            config;
    AssetPipeline           asset_pipeline;
    Project                 project;
};



} // namespace bee