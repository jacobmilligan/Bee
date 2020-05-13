/*
 *  EditorApp.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/Array.hpp"
#include "Bee/Core/Path.hpp"
#include "Bee/Plugins/AssetPipeline/AssetPipeline.hpp"
#include "Bee/Core/Plugin.hpp"


namespace bee {


struct EditorState;

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

    /// Paths relative to the projects root directory
    DynamicArray<Path>              asset_directories;
    DynamicArray<Path>              source_directories;
    Path                            cache_directory;
};

struct ProjectDescriptor
{
    StringView      name;
    StringView      description;
    StringView      cache_root;
    AssetPlatform   platform { AssetPlatform::unknown };
};

#define BEE_EDITOR_MODULE_NAME "BEE_EDITOR_MODULE"

struct EditorModule
{
    bool (*create_project)(const ProjectDescriptor& desc, const Path& directory, Project* dst) { nullptr };

    bool (*create_and_open_project)(const ProjectDescriptor& desc, const Path& directory) { nullptr };

    bool (*delete_project)(const Path& root) { nullptr };

    bool (*open_project)(const Path& root, const AssetPlatform platform) { nullptr };

    bool (*close_project)() { nullptr };

    const Project& (*get_project)() { nullptr };
};


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.Editor/EditorApp.generated.inl"
#endif // BEE_ENABLE_REFLECTION