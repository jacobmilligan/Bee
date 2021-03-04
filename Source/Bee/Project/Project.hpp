/*
 *  Project.hpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Path.hpp"
#include "Bee/Core/Enum.hpp"
#include "Bee/Core/Result.hpp"


namespace bee {


struct ProjectError
{
    enum Enum
    {
        invalid_extension,
        invalid_project_file_path,
        failed_to_create_project_file,
        failed_to_initialize_asset_pipeline,
        failed_to_initialize_sources,
        failed_to_load_plugins,
        count
    };

    BEE_ENUM_STRUCT(ProjectError);

    const char* to_string() const
    {
        BEE_TRANSLATION_TABLE(value, Enum, const char*, Enum::count,
            "Invalid project file extension",                           // invalid_extension
            "No project file was found at the specified path",          // invalid_project_file_path
            "Failed to create new project files and folders",           // failed_to_create_project_files
            "Failed to initialize cache folder and asset pipeline",     // failed_to_initialize_asset_pipeline
            "Failed to initialize source files and CMake project",      // failed_to_initialize_sources
            "Failed to load the projects plugin dependencies",          // failed_to_load_plugins
        );
    }
};

enum class ProjectOpenMode
{
    open_existing,
    open_or_create
};

#define BEE_PROJECT_MODULE_NAME "BEE_PROJECT_MODULE"

struct AssetPipeline;
struct Project;
struct ProjectModule
{
    Result<Project*, ProjectError> (*open)(const PathView& path, const ProjectOpenMode mode) { nullptr };

    void (*close)(Project* project) { nullptr };

    void (*tick)(Project* project) { nullptr };

    AssetPipeline* (*get_asset_pipeline)(Project* project) { nullptr };
};


} // namespace bee