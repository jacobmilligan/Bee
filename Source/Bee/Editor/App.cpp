/*
 *  App.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#include "Bee/Editor/App.hpp"

#include "Bee/Core/Plugin.hpp"

#include "Bee/DataConnection/DataConnection.hpp"
#include "Bee/Gpu/Gpu.hpp"
#include "Bee/Input/Input.hpp"
#include "Bee/Platform/Platform.hpp"
#include "Bee/RenderGraph/RenderGraph.hpp"
#include "Bee/ImGui/ImGui.hpp"


namespace bee {


static constexpr i32 editor_connection_port = 8888;

struct EditorApp
{
    bool            quit_requested { false };

    // Platform resources
    WindowHandle    main_window;

    // GPU resources
    GpuBackend*     gpu { nullptr };
    DeviceHandle    device;
    SwapchainHandle swapchain;

    // Rendering resources
    RenderGraph*    render_graph { nullptr };

    // Data connection
    DataConnection* connection { nullptr };
};

static EditorApp* g_app = nullptr;

// Modules
static PlatformModule*          g_platform = nullptr;
static GpuModule*               g_gpu { nullptr };
static RenderGraphModule*       g_render_graph { nullptr };
static DataConnectionModule*    g_data_connection { nullptr };
static ImGuiModule*             g_imgui { nullptr };

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

    g_imgui->create_context();

    g_app->quit_requested = false;

    return true;
}

void shutdown()
{
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

void tick()
{
    // Close the app if either the window is closed or the apps quit event fired
    if (g_platform->quit_requested() || g_platform->window_close_requested(g_app->main_window))
    {
        g_app->quit_requested = true;
        return;
    }

    // Reset the global per-frame threadsafe temp allocator used by the runtime
    temp_allocator_reset();

    // Poll input for the app and show some info logs for when the keyboard/mouse was triggered
    g_platform->poll_input();

    auto res = g_data_connection->flush(g_app->connection, 1);
    if (!res)
    {
        log_error("Editor data connection error: %s", res.unwrap_error().to_string());
    }
}

bool quit_requested()
{
    return g_app->quit_requested;
}


} // namespace bee


static bee::EditorAppModule g_module{};

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    if (!loader->require_plugin("Bee.Platform", bee::PluginVersion{ 0, 0, 0 }))
    {
        bee::log_error("Missing dependency: Bee.Platform");
        return;
    }

    if (!loader->require_plugin("Bee.VulkanBackend", bee::PluginVersion{ 0, 0, 0 }))
    {
        bee::log_error("Missing dependency: Bee.VulkanBackend");
        return;
    }

    if (!loader->require_plugin("Bee.RenderGraph", bee::PluginVersion{ 0, 0, 0 }))
    {
        bee::log_error("Missing dependency: Bee.RenderGraph");
        return;
    }

    if (!loader->require_plugin("Bee.DataConnection", bee::PluginVersion{ 0, 0, 0 }))
    {
        bee::log_error("Missing dependency: Bee.DataConnection");
        return;
    }

    if (!loader->require_plugin("Bee.ImGui", bee::PluginVersion{ 0, 0, 0 }))
    {
        bee::log_error("Missing dependency: Bee.ImGui");
        return;
    }

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
    }
}

BEE_PLUGIN_VERSION(0, 0, 0)