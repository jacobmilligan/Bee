/*
 *  EditorApp.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/AssetPipeline/AssetCompiler.hpp"


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

    BEE_REFLECT(nonserialized)
    bool                            is_open { false };

    BEE_REFLECT(nonserialized)
    Path                            location;
};


struct EditorInfo
{
    AssetPlatform   asset_platform { AssetPlatform::unknown };
    const char*     initial_project { nullptr };
};


bool read_editor_config(EditorConfig* config);

bool save_editor_config(const EditorConfig& config);

bool init_project(const Project& project);

bool open_project(Project* project, const Path& path, const AssetPlatform force_platform = AssetPlatform::unknown);

bool close_project(Project* project);

int editor_app_run(Project* project);



} // namespace bee