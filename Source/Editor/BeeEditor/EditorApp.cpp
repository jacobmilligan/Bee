/*
 *  EditorApp.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "BeeEditor/EditorApp.hpp"

#include "Bee/Application/AppLoop.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"


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

    if (BEE_FAIL(project.location.exists()))
    {
        return false;
    }

    const auto project_root = project.location.join(project.name.view());

    if (!project_root.exists())
    {
        fs::mkdir(project_root);
    }

    for (const auto& file : fs::read_dir(project_root))
    {
        if (BEE_FAIL_F(file.extension() != g_beeproj_extension, "Cannot init project: a %s file already exists at %s", g_beeproj_extension, file.c_str()))
        {
            return false;
        }
    }

    JSONSerializer serializer(temp_allocator());
    serialize(SerializerMode::writing, &serializer, const_cast<Project*>(&project));

    const auto proj_file_path = project_root.join(project.name.view()).append_extension(g_beeproj_extension);

    if (BEE_FAIL_F(fs::write(proj_file_path, serializer.c_str()), "Cannot init project: failed to writer %s file", g_beeproj_extension))
    {
        return false;
    }

    const auto assets_path = project_root.join("Assets");
    const auto sources_path = project_root.join("Source");
    const auto cache_path = project_root.join("Cache");

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

int editor_app_run(Project* project)
{
    AppDescriptor info{};
    info.app_name = "Bee Editor";

    AppContext ctx{};
    const auto init_result = app_init(info, &ctx);
    if (init_result != EXIT_SUCCESS)
    {
        return init_result;
    }

    EditorConfig config{};
    if (BEE_FAIL(read_editor_config(&config)))
    {
        return EXIT_FAILURE;
    }

    // If we don't have a project - get the last known opened one
    if (!project->is_open)
    {
        if (config.most_recent_project >= 0 && config.most_recent_project < config.projects.size())
        {
            open_project(project, config.projects[config.most_recent_project]);
        }
        else
        {
            log_error("No project!");
        }
    }
    else
    {
        config.most_recent_project = container_index_of(config.projects, [&](const Path& location)
        {
            return location == project->location;
        });

        if (config.most_recent_project < 0)
        {
            config.most_recent_project = config.projects.size();
            config.projects.push_back(project->location);
        }

        save_editor_config(config);
    }

    /*
     * Main loop
     */
    while (platform_is_running() && !platform_quit_requested() && !ctx.quit)
    {
        poll_input(&ctx.default_input);
    }

    app_shutdown();

    return EXIT_SUCCESS;
}


} // namespace bee