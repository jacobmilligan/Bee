/*
 *  App.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#include "Bee/Editor/App.hpp"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Filesystem.hpp"

#include "Bee/DataConnection/DataConnection.hpp"
#include "Bee/Gpu/Gpu.hpp"
#include "Bee/Input/Input.hpp"
#include "Bee/Platform/Platform.hpp"
#include "Bee/RenderGraph/RenderGraph.hpp"
#include "Bee/ImGui/ImGui.hpp"
#include "Bee/AssetPipeline/AssetPipeline.hpp"
#include "Bee/ShaderPipeline/ShaderPipeline.hpp"


namespace bee {


static constexpr i32 editor_connection_port = 8888;

struct EditorApp
{
    ImGuiBackend*       imgui_backend { nullptr };

    // Platform resources
    WindowHandle        main_window;

    // GPU resources
    SwapchainHandle     swapchain;
    GpuBackend*         gpu { nullptr };
    DeviceHandle        device;

    bool                quit_requested { false };
    bool                reloaded { false };

    BEE_PAD(2);

    // Rendering resources
    RenderGraph*        render_graph { nullptr };
    RenderGraphPass*    imgui_pass { nullptr };

    // Data connection
    AssetPipeline*      asset_pipeline { nullptr };
    DataConnection*     connection { nullptr };
};

static EditorApp* g_app = nullptr;

// Modules
static PlatformModule*          g_platform = nullptr;
static GpuModule*               g_gpu { nullptr };
static RenderGraphModule*       g_render_graph { nullptr };
static DataConnectionModule*    g_data_connection { nullptr };
static ImGuiModule*             g_imgui { nullptr };
static ImGuiBackendModule*      g_imgui_backend { nullptr };
static AssetPipelineModule*     g_asset_pipeline { nullptr };
static ShaderPipelineModule*    g_shader_pipeline { nullptr };

/*
 ******************************************************
 *
 * ImGui render pass
 *
 ******************************************************
 */
static void init_pass(GpuBackend* gpu, const DeviceHandle device, const void* external_data, void* pass_data)
{
    auto res = g_imgui_backend->create_backend(device, gpu, g_app->asset_pipeline, system_allocator());
    if (!res)
    {
        log_error("%s", res.unwrap_error().to_string());
        return;
    }
    g_app->imgui_backend = res.unwrap();
}

static void destroy_pass(GpuBackend* gpu, const DeviceHandle device, const void* external_data, void* pass_data)
{
    g_imgui_backend->destroy_backend(g_app->imgui_backend);
}

static void setup_pass(RenderGraphPass* pass, RenderGraphBuilderModule* builder, const void* external_data, void* pass_data)
{
    const auto backbuffer = builder->import_backbuffer(pass, "Backbuffer", g_app->swapchain);
    builder->write_color(pass, backbuffer, LoadOp::clear, StoreOp::store, 1);
}

static void execute_pass(
    RenderGraphPass* pass,
    RenderGraphStorage* storage,
    GpuCommandBackend* cmd,
    CommandBuffer* cmdbuf,
    const void* external_data,
    void* pass_data
)
{
    // Get the concrete GPU resources from the virtual RenderGraphPass object
    const TextureViewHandle* attachments = nullptr;
    const u32 attachment_count = storage->get_attachments(pass, &attachments);

    // All draw calls etc. must take place within a render pass and because we're using the render graph we can
    // just use the automatically-created one for this pass
    ClearValue clear_value(0.3f, 0.3f, 0.3f, 1.0f);
    cmd->begin_render_pass(
        cmdbuf,
        storage->get_gpu_pass(pass),
        storage->get_backbuffer_rect(pass),
        attachment_count, attachments,
        attachment_count, &clear_value
    );
    g_imgui_backend->draw(g_app->imgui_backend, cmdbuf);
    cmd->end_render_pass(cmdbuf);
}

/*
 ******************************************************
 *
 * Main editor app loop - startup, shutdown, tick
 *
 ******************************************************
 */
bool startup()
{
    g_app->quit_requested = true;

    // Initialize the OS + app exe and register input default input devices
    if (!g_platform->start("Bee.Sandbox"))
    {
        log_error("Failed to initialize platform");
        return false;
    }

    // Create the main app window
    WindowCreateInfo window_info{};
    window_info.title = "Bee Sandbox";
    window_info.monitor = g_platform->get_primary_monitor()->handle;
    g_app->main_window = g_platform->create_window(window_info);
    if (!g_app->main_window.is_valid())
    {
        log_error("Failed to create main editor window");
        return false;
    }

    // Initialize Vulkan backend and device
    g_app->gpu = g_gpu->get_default_backend(GpuApi::vulkan);

    if (g_app->gpu == nullptr || !g_app->gpu->init())
    {
        log_error("Failed to load Vulkan backend");
        return false;
    }

    g_app->device = g_app->gpu->create_device(DeviceCreateInfo{0 });
    if (!g_app->device.is_valid())
    {
        log_error("Failed to create Vulkan device");
        return false;
    }

    const auto fb_size = g_platform->get_framebuffer_size(g_app->main_window);
    // create a new swapchain for presenting the final backbuffer
    SwapchainCreateInfo swapchain_info{};
    swapchain_info.vsync = true;
    swapchain_info.window = g_app->main_window;
    swapchain_info.debug_name = "SandboxSwapchain";
    swapchain_info.texture_format = PixelFormat::rgba8;
    swapchain_info.texture_extent.width = sign_cast<u32>(fb_size.x);
    swapchain_info.texture_extent.height = sign_cast<u32>(fb_size.y);
    g_app->swapchain = g_app->gpu->create_swapchain(g_app->device, swapchain_info);

    if (!g_app->swapchain.is_valid())
    {
        log_error("Failed to create swapchain");
        return false;
    }

    // Initialize the editor shader cache
    Path cache_root = fs::roots().data.view();
    cache_root.append("EditorCache");

    AssetPipelineImportInfo import_info{};
    import_info.name = "Editor";
    import_info.cache_root = cache_root.view();

    AssetPipelineInfo asset_pipeline_info{};
    asset_pipeline_info.import = &import_info;
    asset_pipeline_info.flags = AssetPipelineFlags::import | AssetPipelineFlags::load;

    auto res = g_asset_pipeline->create_pipeline(asset_pipeline_info);
    if (!res)
    {
        log_error("Failed to create asset pipeline: %s", res.unwrap_error().to_string());
        return false;
    }
    g_app->asset_pipeline = res.unwrap();

    // Init the shader pipeline
    g_shader_pipeline->init(g_app->asset_pipeline, g_app->gpu, g_app->device);

    // Create a new render graph to process the frame - manages creating GPU resources, automatic barriers etc.
    g_app->render_graph = g_render_graph->create_graph(g_app->gpu, g_app->device);
    if (g_app->render_graph == nullptr)
    {
        log_error("Failed to create editor render graph");
        return false;
    }

    // Now that all the main module are initialized, create a data connection for the runtime
    auto startup_res = g_data_connection->startup();
    if (!startup_res)
    {
        log_error("%s", startup_res.unwrap_error().to_string());
        return false;
    }

    auto dc_res = g_data_connection->create_server(SocketAddressFamily::ipv4, BEE_IPV4_LOCALHOST, editor_connection_port);
    if (!dc_res)
    {
        log_error("%s", dc_res.unwrap_error().to_string());
        return false;
    }

    g_app->connection = dc_res.unwrap();

    g_app->quit_requested = false;

    return true;
}

void shutdown()
{
    // Non-core modules - these *may* have assets associated with them so we need to do this before the final asset
    // pipeline refresh
    if (g_app->imgui_pass != nullptr)
    {
        g_render_graph->remove_pass(g_app->imgui_pass);
    }

    // Core modules - these don't have assets associated with them so it's safe to do the final refresh here
    g_asset_pipeline->refresh(g_app->asset_pipeline);

    // Safe to shut down importers/loaders/locators now
    g_shader_pipeline->shutdown();

    if (g_app->connection != nullptr)
    {
        auto res = g_data_connection->destroy_connection(g_app->connection);
        if (!res)
        {
            log_error("Failed to destroy editor server connection: %s", res.unwrap_error().to_string());
        }

        res = g_data_connection->shutdown();
        if (!res)
        {
            log_error("%s", res.unwrap_error().to_string());
        }
    }

    // Destroy rendergraph and then GPU
    if (g_app->gpu != nullptr && g_app->gpu->is_initialized())
    {
        if (g_app->render_graph != nullptr)
        {
            g_render_graph->destroy_graph(g_app->render_graph);
        }

        if (g_app->device.is_valid())
        {
            // the submissions will have already been flushed by destroying the render graph
            if (g_app->swapchain.is_valid())
            {
                g_app->gpu->destroy_swapchain(g_app->device, g_app->swapchain);
                g_app->swapchain = SwapchainHandle{};
            }

            g_app->gpu->destroy_device(g_app->device);
            g_app->device = DeviceHandle{};
        }

        // Destroy the gpu backend
        g_app->gpu->destroy();
    }

    if (g_app->main_window.is_valid())
    {
        g_platform->destroy_window(g_app->main_window);
    }

    if (g_platform->is_running())
    {
        g_platform->shutdown();
    }
}

static void show_tree(const PathView& root)
{
    StaticString<1024> buf(root.string_view());
    if (!g_imgui->TreeNodeStr(buf.data()))
    {
        return;
    }

    for (const auto& file : fs::read_dir(root))
    {
        if (fs::is_dir(file))
        {
            show_tree(file);
        }
    }

    g_imgui->TreePop();
}

void tick()
{
    // Close the app if either the window is closed or the apps quit event fired
    if (g_platform->quit_requested() || g_platform->window_close_requested(g_app->main_window))
    {
        g_app->quit_requested = true;
        return;
    }

    // Poll input before doing any other processing in case we want to close app
    g_platform->poll_input();

    // Add the imgui render graph pass here instead of init() in case the plugin was reloaded
    if (g_app->imgui_pass == nullptr)
    {
        g_app->imgui_pass = g_render_graph->add_pass(
            g_app->render_graph,
            "ImGuiPass",
            setup_pass,
            execute_pass,
            init_pass,
            destroy_pass
        );
    }

    auto ap_res = g_asset_pipeline->refresh(g_app->asset_pipeline);
    if (!ap_res)
    {
        log_error("Asset pipeline error: %s", ap_res.unwrap_error().to_string());
    }

    if (g_app->imgui_backend != nullptr)
    {
        g_imgui_backend->new_frame(g_app->imgui_backend, g_app->main_window);
        g_imgui->Begin("A window frame", nullptr, 0);
#if 1
        g_imgui->Button("Button", ImVec2 { 60, 40 });
        g_imgui->Text("Some text too?");
        g_imgui->Button("Also another button", ImVec2 { 100, 40 });
        show_tree(get_plugin_source_path("Bee.ImGui"));
#endif // 0
        g_imgui->End();
        g_imgui->Render();
    }

    g_render_graph->setup(g_app->render_graph);
    g_render_graph->execute(g_app->render_graph);
    g_app->gpu->commit_frame(g_app->device);
}

bool quit_requested()
{
    return g_app->quit_requested;
}


} // namespace bee


static bee::EditorAppModule g_module{};

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    bee::g_app = loader->get_static<bee::EditorApp>("Bee.EditorApp");

    g_module.startup = bee::startup;
    g_module.shutdown = bee::shutdown;
    g_module.tick = bee::tick;
    g_module.quit_requested = bee::quit_requested;
    loader->set_module(BEE_EDITOR_APP_MODULE_NAME, &g_module, state);

    if (state == bee::PluginState::loading)
    {
        bee::g_gpu = static_cast<bee::GpuModule*>(loader->get_module(BEE_GPU_MODULE_NAME));
        bee::g_platform = static_cast<bee::PlatformModule*>(loader->get_module(BEE_PLATFORM_MODULE_NAME));
        bee::g_render_graph = static_cast<bee::RenderGraphModule*>(loader->get_module(BEE_RENDER_GRAPH_MODULE_NAME));
        bee::g_data_connection = static_cast<bee::DataConnectionModule*>(loader->get_module(BEE_DATA_CONNECTION_MODULE_NAME));
        bee::g_imgui = static_cast<bee::ImGuiModule*>(loader->get_module(BEE_IMGUI_MODULE_NAME));
        bee::g_imgui_backend = static_cast<bee::ImGuiBackendModule*>(loader->get_module(BEE_IMGUI_BACKEND_MODULE_NAME));
        bee::g_asset_pipeline = static_cast<bee::AssetPipelineModule*>(loader->get_module(BEE_ASSET_PIPELINE_MODULE_NAME));
        bee::g_shader_pipeline = static_cast<bee::ShaderPipelineModule*>(loader->get_module(BEE_SHADER_PIPELINE_MODULE_NAME));

        if (bee::g_app->imgui_pass != nullptr)
        {
            bee::g_render_graph->remove_pass(bee::g_app->imgui_pass);
            bee::g_app->imgui_pass = nullptr;
        }
    }
}
