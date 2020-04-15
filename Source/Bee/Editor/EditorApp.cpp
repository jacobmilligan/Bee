/*
 *  EditorApp.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Editor/EditorApp.hpp"

#include "Bee/Application/Application.hpp"
#include "Bee/AssetPipeline/AssetPipeline.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"
#include "Bee/Core/Time.hpp"
#include "Bee/Plugins/DefaultAssets/DefaultAssets.hpp"
#include "Bee/Plugins/ImGui/ImGui.hpp"

namespace bee {


static constexpr auto g_beeproj_extension = ".beeproj";
static Path g_config_path = fs::get_appdata().data_root.join("Editor.json");


bool read_editor_config(EditorConfig* config)
{
    if (!g_config_path.exists())
    {
        return true;
    }

    auto contents = fs::read(g_config_path, temp_allocator());
    JSONSerializer serializer(contents.data(), rapidjson::ParseFlag::kParseInsituFlag, temp_allocator());
    serialize(SerializerMode::reading, &serializer, config);
    return true;
}

bool save_editor_config(const EditorConfig& config)
{
    JSONSerializer serializer(temp_allocator());
    serialize(SerializerMode::writing, &serializer, const_cast<EditorConfig*>(&config));
    return fs::write(g_config_path, serializer.c_str());
}


bool init_project(const Project& project)
{
    if (BEE_FAIL(project.platform != AssetPlatform::unknown))
    {
        return false;
    }

    if (BEE_FAIL(!project.name.empty()))
    {
        return false;
    }

    if (BEE_FAIL(project.location.parent().exists()))
    {
        return false;
    }

    if (!project.location.exists())
    {
        fs::mkdir(project.location);
    }

    for (const auto& file : fs::read_dir(project.location))
    {
        if (file.extension() == g_beeproj_extension)
        {
            log_error("A Bee Project file already exists at %s", file.c_str());
            return false;
        }
    }

    JSONSerializer serializer(temp_allocator());
    serialize(SerializerMode::writing, &serializer, const_cast<Project*>(&project));

    const auto proj_file_path = project.location.join(project.name.view()).append_extension(g_beeproj_extension);

    if (BEE_FAIL_F(fs::write(proj_file_path, serializer.c_str()), "Cannot init project: failed to write %s file", g_beeproj_extension))
    {
        return false;
    }

    const auto assets_path = project.location.join(project.assets_root);
    const auto sources_path = project.location.join(project.sources_root);
    const auto cache_path = project.location.join(project.cache_root);

    if (!assets_path.exists())
    {
        fs::mkdir(assets_path);
    }

    if (!sources_path.exists())
    {
        fs::mkdir(sources_path);
    }

    if (!cache_path.exists())
    {
        fs::mkdir(cache_path);
    }

    return true;
}


bool open_project(Project* project, const Path& path, const AssetPlatform force_platform)
{
    if (project->is_open)
    {
        close_project(project);
    }
    
    if (BEE_FAIL_F(path.exists() && path.extension() == g_beeproj_extension, "%s is not a .beeproj file", path.c_str()))
    {
        return false;
    }

    for (const auto& file : fs::read_dir(path.parent()))
    {
        if (file.extension() == g_beeproj_extension)
        {
            if (BEE_CHECK_F(project->location.empty(), "Unable to read project file: There are multiple %s files at %s", g_beeproj_extension, path.c_str()))
            {
                project->location = file;
                project->location.make_generic();
            }
        }
    }

    if (BEE_FAIL_F(!project->location.empty(), "Could not find a valid %s file at %s", g_beeproj_extension, path.parent().c_str()))
    {
        return false;
    }

    auto contents = fs::read(project->location, temp_allocator());
    JSONSerializer serializer(contents.data(), rapidjson::ParseFlag::kParseInsituFlag, temp_allocator());
    serialize(SerializerMode::reading, &serializer, project);

    project->is_open = true;
    if (force_platform != AssetPlatform::unknown)
    {
        project->platform = force_platform;
    }
    return true;
}

bool close_project(Project* project)
{
    if (!project->is_open)
    {
        return false;
    }

    destruct(project);
    project->is_open = false;
    return true;
}

/*
 *********************
 *
 * Editor App Loop
 *
 *********************
 */
int editor_on_launch(AppContext* ctx)
{
    auto* editor = static_cast<EditorContext*>(ctx->user_data);
    const auto& launch = editor->launch_params;
    auto& project = editor->project;
    auto& config = editor->config;

    if (BEE_FAIL(read_editor_config(&editor->config)))
    {
        return EXIT_FAILURE;
    }

    // Handle the launch modes
    if (launch.mode == EditorLaunchMode::existing_project)
    {
        if (!open_project(&editor->project, launch.project_path, launch.platform))
        {
            return EXIT_FAILURE;
        }
    }
    else if (launch.mode == EditorLaunchMode::new_project)
    {
        project.engine_version = BEE_VERSION;
        project.name = launch.project_name;
        project.location.append(launch.project_path).append(launch.project_name);
        project.platform = launch.platform;
        project.assets_root = "Assets";
        project.sources_root = "Source";
        project.cache_root = "Cache";

        if (!init_project(project))
        {
            return EXIT_FAILURE;
        }
    }
    else
    {
        // use standard mode - open the last known project
        if (config.most_recent_project >= 0 && config.most_recent_project < config.projects.size())
        {
            if (!open_project(&project, config.projects[config.most_recent_project]))
            {
                return EXIT_FAILURE;
            }
        }
        else
        {
            log_error("Bee: no valid project specified or found in the recent projects cache");
            return EXIT_FAILURE;
        }
    }

    // init asset pipeline before loading any plugins etc.
    AssetPipelineInitInfo asset_pipeline_info{};
    asset_pipeline_info.platform = project.platform;
    asset_pipeline_info.assets_source_root = project.location.join(project.assets_root);
    asset_pipeline_info.asset_database_name = "AssetDB";
    asset_pipeline_info.asset_database_directory = project.location.join(project.cache_root);

    editor->asset_pipeline.init(asset_pipeline_info);

    // load the default assets plugin
    load_plugin(BEE_DEFAULT_ASSETS_PIPELINE_PLUGIN_NAME);

    // Load the ImGui plugin
    load_plugin(BEE_IMGUI_ASSET_PIPELINE_PLUGIN_NAME);
    load_plugin(BEE_IMGUI_PLUGIN_NAME);

    return save_editor_config(config) ? EXIT_SUCCESS : EXIT_FAILURE;
}

void editor_on_frame(AppContext* ctx)
{
    poll_input(&ctx->default_input);

    if (is_window_close_requested(ctx->main_window))
    {
        ctx->quit = true;
        return;
    }

    current_thread::sleep(make_time_point<TimeInterval::milliseconds>(8).ticks());
}

void editor_on_shutdown(AppContext* ctx)
{
    auto* editor = static_cast<EditorContext*>(ctx->user_data);

    unload_plugin(BEE_IMGUI_ASSET_PIPELINE_PLUGIN_NAME);

    if (editor->project.is_open)
    {
        close_project(&editor->project);
    }

    // destroy asset pipeline last in case plugins need to use it when unloading
    editor->asset_pipeline.destroy();
}

void editor_on_fail(AppContext* ctx)
{
    auto* editor = static_cast<EditorContext*>(ctx->user_data);

    if (editor->project.is_open)
    {
        close_project(&editor->project);
    }

    editor->asset_pipeline.destroy();
}

/*
 **************************
 *
 * Editor initialization
 *
 **************************
 */
int editor_app_run(const EditorLaunchParameters& params)
{
    EditorContext editor;
    editor.launch_params = params;

    AppDescriptor desc{};
    desc.app_name = "Bee Editor";
    desc.user_data = &editor;
    desc.on_launch = editor_on_launch;
    desc.on_frame = editor_on_frame;
    desc.on_shutdown = editor_on_shutdown;
    desc.on_fail = editor_on_fail;

    return app_run(desc);
}


} // namespace bee