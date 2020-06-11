/*
 *  EditorApp.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Editor/EditorApp.hpp"

#include "Bee/Bee.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"
#include "Bee/Core/Time.hpp"
#include "Bee/Plugins/AssetPipeline/AssetPipeline.hpp"
#include "Bee/Plugins/AssetRegistry/AssetRegistry.hpp"
#include "Bee/Plugins/Renderer/Renderer.hpp"
#include "Bee/Plugins/ImGui/ImGui.hpp"
#include "Bee/Core/CLI.hpp"

namespace bee {


static constexpr auto g_beeproj_extension = ".beeproj";


struct Application // NOLINT
{
    WindowHandle        main_window;
    InputBuffer         input_buffer;
    AssetPipeline*      pipeline { nullptr };
};

struct Editor
{
    Path                config_path;
    EditorConfig        config;
    Project             project;
    bool                is_project_open { false };
    Path                project_location;
};

static Editor*              g_editor { nullptr };
static ImGuiModule*         g_imgui { nullptr };
static AssetPipelineModule* g_asset_pipeline { nullptr };
static AssetRegistryModule* g_asset_registry { nullptr };
static RendererModule*      g_renderer { nullptr };


bool read_editor_config()
{
    if (!g_editor->config_path.exists())
    {
        return true;
    }

    auto contents = fs::read(g_editor->config_path, temp_allocator());
    JSONSerializer serializer(contents.data(), rapidjson::ParseFlag::kParseInsituFlag, temp_allocator());
    serialize(SerializerMode::reading, &serializer, &g_editor->config);
    return true;
}

bool save_editor_config()
{
    JSONSerializer serializer(temp_allocator());
    serialize(SerializerMode::writing, &serializer, &g_editor->config);
    return fs::write(g_editor->config_path, serializer.c_str());
}

bool close_project();

bool init_project(const Path& location, const AssetPlatform force_platform)
{
    if (g_editor->is_project_open)
    {
        return false;
    }

    g_editor->project_location.clear();
    g_editor->project_location.append(location);

    auto& project = g_editor->project;

    if (force_platform != AssetPlatform::unknown)
    {
        project.platform = force_platform;
    }

    BEE_ASSERT(project.platform != AssetPlatform::unknown);

    // TODO(Jacob): initialize the projects asset pipeline

    g_editor->is_project_open = true;

    return save_editor_config();
}

bool create_project(const ProjectDescriptor& desc, const Path& directory, Project* dst)
{
    if (BEE_FAIL(desc.platform != AssetPlatform::unknown))
    {
        return false;
    }

    if (BEE_FAIL(!desc.name.empty()))
    {
        return false;
    }

    if (BEE_FAIL(directory.parent_path().exists()))
    {
        return false;
    }

    if (!directory.exists())
    {
        fs::mkdir(directory);
    }

    for (const auto& file : fs::read_dir(directory))
    {
        if (file.extension() == g_beeproj_extension)
        {
            log_error("A Bee Project file already exists at %s", file.c_str());
            return false;
        }
    }

    dst->description.clear();
    dst->asset_directories.clear();
    dst->source_directories.clear();
    dst->cache_directory.clear();

    dst->name = desc.name;
    dst->engine_version = BEE_VERSION;
    dst->platform = desc.platform;
    dst->description.append(desc.description);
    dst->cache_directory.append(desc.cache_root);

    JSONSerializer serializer(temp_allocator());
    serialize(SerializerMode::writing, &serializer, dst);

    const auto proj_file_path = directory.join(desc.name).append_extension(g_beeproj_extension);

    if (BEE_FAIL_F(fs::write(proj_file_path, serializer.c_str()), "Cannot init desc: failed to write %s file", g_beeproj_extension))
    {
        return false;
    }

    const auto cache_path = directory.join(desc.cache_root, temp_allocator());

//    if (!assets_path.exists())
//    {
//        fs::mkdir(assets_path);
//    }
//
//    if (!sources_path.exists())
//    {
//        fs::mkdir(sources_path);
//    }

    if (!cache_path.exists())
    {
        fs::mkdir(cache_path);
    }

    return true;
}

bool create_and_open_project(const ProjectDescriptor& desc, const Path& directory)
{
    if (g_editor->is_project_open)
    {
        if (!close_project())
        {
            return false;
        }
    }

    if (!create_project(desc, directory, &g_editor->project))
    {
        return false;
    }

    return init_project(directory, desc.platform);
}

bool delete_project(const Path& root)
{
    if (!root.exists())
    {
        return false;
    }

    if (!fs::rmdir(root, true))
    {
        return false;
    }

    const auto index = find_index_if(g_editor->config.projects, [&](const Path& p)
    {
        return p == root;
    });

    if (index >= 0)
    {
        g_editor->config.projects.erase(index);
        g_editor->config.most_recent_project = g_editor->config.projects.empty() ? -1 : 0;

        if (!save_editor_config())
        {
            return false;
        }
    }

    return true;
}

bool open_project(const Path& path, const AssetPlatform force_platform)
{
    if (g_editor->is_project_open)
    {
        close_project();
    }

    if (BEE_FAIL_F(path.exists() && path.extension() == g_beeproj_extension, "%s is not a .beeproj file", path.c_str()))
    {
        return false;
    }

    for (const auto& file : fs::read_dir(path.parent_view()))
    {
        if (file.extension() == g_beeproj_extension)
        {
            if (BEE_CHECK_F(g_editor->project_location.empty(), "Unable to read project file: There are multiple %s files at %s", g_beeproj_extension, path.c_str()))
            {
                g_editor->project_location = file;
                break;
            }
        }
    }

    if (BEE_FAIL_F(!g_editor->project_location.empty(), "Could not find a valid %s file at %" BEE_PRIsv, g_beeproj_extension, BEE_FMT_SV(path.parent_view())))
    {
        return false;
    }

    auto& project = g_editor->project;
    auto contents = fs::read(g_editor->project_location, temp_allocator());
    JSONSerializer serializer(contents.data(), rapidjson::ParseFlag::kParseInsituFlag, temp_allocator());
    serialize(SerializerMode::reading, &serializer, &project);

    return init_project(path, force_platform);
}

bool close_project()
{
    if (!g_editor->is_project_open)
    {
        return false;
    }

    // TODO(Jacob): destroy project asset pipeline

    destruct(&g_editor->project);

    g_editor->is_project_open = false;

    return true;
}

/*
 *********************
 *
 * Editor App Loop
 *
 *********************
 */
int launch_application(Application* app, int argc, char** argv)
{
    if (g_asset_pipeline->init == nullptr)
    {
        log_error("Asset Pipeline plugin is required but not registered");
        return EXIT_FAILURE;
    }

    if (g_editor->config_path.empty())
    {
        g_editor->config_path = bee::fs::get_root_dirs().data_root.join("Editor.json");
    }

    /*
     * positionals & options common to all project subcommands
     */
    cli::Positional positionals[] =
    {
        { "location", "Full path to the folder containing the .beeproj.json file to open" }
    };

    cli::Option options[] =
    {
        { 'p', "platform", false, "The platform to use for the project", 1 }
    };

    cli::ParserDescriptor cmd_parser("bee", positionals, options);

    const auto parser = cli::parse(argc, argv, cmd_parser);

    if (parser.help_requested && argc != 1)
    {
        log_info("%s", parser.requested_help_string);
        return EXIT_FAILURE;
    }

    // Handle open command

    if (!parser.positionals.empty())
    {
        const auto* project_location = cli::get_positional(parser, 0);

        auto platform = AssetPlatform::unknown;

        if (cli::has_option(parser, "platform"))
        {
            platform = enum_from_string<AssetPlatform>(cli::get_option(parser, "platform"));
        }

        if (!open_project(project_location, platform))
        {
            log_error("Failed to open project %s", project_location);
            return EXIT_FAILURE;
        }
    }
    else
    {
        log_info("Launching editor without a project");
    }

    if (BEE_FAIL(read_editor_config()))
    {
        return EXIT_FAILURE;
    }

    if (!platform_launch("Bee Editor"))
    {
        return EXIT_FAILURE;
    }

    // Initialize input
    input_buffer_init(&app->input_buffer);

    // Create main window
    WindowConfig window_config{};
    window_config.title = "Bee";
    app->main_window = create_window(window_config);

    // Ensure that the editor data folder for this version exists
    // TODO(Jacob): replace BEE_VERSION with EDITOR_VERSION or similar
    const auto editor_data_dir = fs::get_root_dirs().data_root.join("Editor" BEE_VERSION);

    if (!editor_data_dir.exists())
    {
        fs::mkdir(editor_data_dir);
    }

    g_asset_registry->init();

    // Initialize the editors asset pipeline
    AssetPipelineInitInfo info{};
    info.platform = default_asset_platform;
    info.project_root = editor_data_dir;
    info.cache_directory = "Cache";
    info.asset_database_name = "AssetDB";

    app->pipeline = g_asset_pipeline->init(info, system_allocator());

    if (app->pipeline == nullptr)
    {
        g_asset_registry->destroy();
        return false;
    }

    // initialize the renderer after the pipeline/registry are all setup
    DeviceCreateInfo device_info{};
    device_info.physical_device_id = 0;
    g_renderer->init(device_info);
    g_renderer->add_swapchain(SwapchainKind::primary, app->main_window, PixelFormat::bgra8, "EditorWindow");

    // initialize non-core plugins
    g_imgui->init();

    return app->main_window.is_valid() ? EXIT_SUCCESS : EXIT_FAILURE;
}

ApplicationState tick_application(Application* app)
{
    poll_input(&app->input_buffer);

    g_asset_pipeline->refresh(app->pipeline);

    if (is_window_close_requested(app->main_window))
    {
        return ApplicationState::quit_requested;
    }

    g_renderer->execute_frame();

    current_thread::sleep(make_time_point<TimeInterval::milliseconds>(8).ticks());
    return ApplicationState::running;
}

void shutdown_application(Application* app)
{
    g_imgui->destroy();
    g_renderer->destroy();
    g_asset_pipeline->destroy(app->pipeline);
    g_asset_registry->destroy();

    destroy_window(app->main_window);

    if (g_editor->is_project_open)
    {
        close_project();
    }
}

void fail_application(Application* app)
{
    if (app->pipeline != nullptr)
    {
        g_asset_pipeline->destroy(app->pipeline);
        app->pipeline = nullptr;
    }

    if (app->main_window.is_valid())
    {
        destroy_window(app->main_window);
    }

    if (g_editor->is_project_open)
    {
        close_project();
    }
}


} // namespace bee


static bee::ApplicationModule g_app_interface{};
static bee::EditorModule g_editor_interface{};


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    bee::g_asset_pipeline = registry->get_module<bee::AssetPipelineModule>(BEE_ASSET_PIPELINE_MODULE_NAME);
    bee::g_asset_registry = registry->get_module<bee::AssetRegistryModule>(BEE_ASSET_REGISTRY_MODULE_NAME);
    bee::g_renderer = registry->get_module<bee::RendererModule>(BEE_RENDERER_MODULE_NAME);

    bee::g_imgui = registry->get_module<bee::ImGuiModule>(BEE_IMGUI_MODULE_NAME);

    g_app_interface.instance = registry->get_or_create_persistent<bee::Application>("BeeEditorApplication");
    g_app_interface.launch = bee::launch_application;
    g_app_interface.shutdown = bee::shutdown_application;
    g_app_interface.tick = bee::tick_application;
    g_app_interface.fail = bee::fail_application;

    bee::g_editor = registry->get_or_create_persistent<bee::Editor>("BeeEditorData");
    g_editor_interface.create_project = bee::create_project;
    g_editor_interface.delete_project = bee::delete_project;
    g_editor_interface.open_project = bee::open_project;
    g_editor_interface.close_project = bee::close_project;

    registry->toggle_module(state, BEE_APPLICATION_MODULE_NAME, &g_app_interface);
    registry->toggle_module(state, BEE_EDITOR_MODULE_NAME, &g_editor_interface);
}