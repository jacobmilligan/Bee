/*
 *  Project.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#include "Bee/Project/Project.hpp"
#include "Bee/Project/Project.inl"

#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"

#include "Bee/AssetPipeline/AssetPipeline.hpp"


namespace bee {


static constexpr const char* g_default_cache_folder_name = "Cache";
static constexpr const char* g_default_source_folder_name = "Source";

static AssetPipelineModule* g_asset_pipeline = nullptr;

struct Project
{
    ProjectDescriptor   descriptor;
    AssetPipeline*      asset_pipeline { nullptr };
};

static bool init_project_file(const PathView& path, ProjectDescriptor* desc)
{
    if (!path.exists())
    {
        // make the parent directory if not already created
        if (!path.parent().exists() && !fs::mkdir(path.parent(), true))
        {
            return false;
        }

        desc->name = path.stem();
        // Setup default cache path
        desc->cache_root = g_default_cache_folder_name;
        // Setup default source path
        desc->source_root = g_default_source_folder_name;

        JSONSerializer serializer(temp_allocator());
        serialize(SerializerMode::writing, &serializer, desc, temp_allocator());
        if (!fs::write_all(path, serializer.c_str()))
        {
            return false;
        }
    }
    else
    {
        auto contents = fs::read_all_text(path, temp_allocator());
        JSONSerializer serializer(contents.data(), JSONSerializeFlags::parse_in_situ, temp_allocator());
        serialize(SerializerMode::reading, &serializer, desc, temp_allocator());
    }

    desc->full_path = path.parent();
    return true;
}

static Result<AssetPipeline*, AssetPipelineError> init_cache(const ProjectDescriptor& desc)
{
    const auto full_cache_root = desc.full_path.join(desc.cache_root.view());

    AssetPipelineImportInfo import{};
    import.cache_root = full_cache_root.view();
    import.name = desc.name.view();
    if (!desc.asset_roots.empty())
    {
        import.source_root_count = desc.asset_roots.size();
        import.source_roots = BEE_ALLOCA_ARRAY(PathView, desc.asset_roots.size());
        for (int i = 0; i < desc.asset_roots.size(); ++i)
        {
            import.source_roots[i] = desc.asset_roots[i].view();
        }
    }

    AssetPipelineInfo info{};
    info.flags = AssetPipelineFlags::load | AssetPipelineFlags::import;
    info.import = &import;

    return g_asset_pipeline->create_pipeline(info);
}

static bool init_sources(const ProjectDescriptor& desc)
{
    const auto full_source_path = desc.full_path.join(desc.source_root.view());

    if (!full_source_path.exists())
    {
        if (!fs::mkdir(full_source_path.view()))
        {
            return false;
        }
    }

    static constexpr const char* default_cmake = R"(cmake_minimum_required(VERSION 3.15)
project(%s)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake)

include(%s/project.cmake)

bee_begin()
bee_end()
)";

    const auto cmakelists = full_source_path.join("CMakeLists.txt", temp_allocator());
    const auto cmake_folder = fs::roots().installation.join("CMake", temp_allocator());

    if (!cmake_folder.exists() && !fs::mkdir(cmake_folder.view()))
    {
        return false;
    }

    if (!cmakelists.exists())
    {
        auto formatted = str::format(default_cmake, desc.name.c_str(), cmake_folder.c_str());
        if (!fs::write_all(cmakelists.view(), formatted.view()))
        {
            return false;
        }
    }

    return true;
}

static bool init_plugins(const ProjectDescriptor& desc)
{
    PluginLoader loader{};
    for (const auto& plugin : desc.plugins)
    {
        if (!loader.require_plugin(plugin.name.view(), plugin.version))
        {
            return false;
        }
    }

    return true;
}

Result<Project*, ProjectError> open(const PathView& path, const ProjectOpenMode mode)
{
    if (path.extension() != ".bee")
    {
        return { ProjectError::invalid_extension };
    }

    // Create required project files and folders if the project doesn't exist
    if (!path.exists() && mode != ProjectOpenMode::open_or_create)
    {
        return { ProjectError::invalid_project_file_path };
    }

    ProjectDescriptor desc;
    if (!init_project_file(path, &desc))
    {
        return { ProjectError::failed_to_create_project_file };
    }

    auto asset_pipeline = init_cache(desc);
    if (!asset_pipeline)
    {
        log_error("%s", asset_pipeline.unwrap_error().to_string());
        return { ProjectError::failed_to_create_project_file };
    }

    if (!init_sources(desc))
    {
        return { ProjectError::failed_to_initialize_sources };
    }

    if (!init_plugins(desc))
    {
        return { ProjectError::failed_to_load_plugins };
    }

    auto* project = BEE_NEW(system_allocator(), Project);
    project->descriptor = BEE_MOVE(desc);
    project->asset_pipeline = asset_pipeline.unwrap();

    return project;
}

void close(Project* project)
{
    // do a final refresh of the asset pipeline in case any resources etc. need to be released
    g_asset_pipeline->refresh(project->asset_pipeline);
    g_asset_pipeline->destroy_pipeline(project->asset_pipeline);
    BEE_DELETE(system_allocator(), project);
}

void tick(Project* project)
{
    auto res = g_asset_pipeline->refresh(project->asset_pipeline);
    if (!res)
    {
        log_error("Asset pipeline error: %s", res.unwrap_error().to_string());
    }
}

AssetPipeline* get_asset_pipeline(Project* project)
{
    return project->asset_pipeline;
}


} // namespace bee


static bee::ProjectModule g_module{};

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    g_module.open = bee::open;
    g_module.close = bee::close;
    g_module.tick = bee::tick;
    g_module.get_asset_pipeline = bee::get_asset_pipeline;
    loader->set_module(BEE_PROJECT_MODULE_NAME, &g_module, state);

    if (state == bee::PluginState::loading)
    {
        bee::g_asset_pipeline = static_cast<bee::AssetPipelineModule*>(loader->get_module(BEE_ASSET_PIPELINE_MODULE_NAME));
    }
}