/*
 *  Main.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Main.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Math/float4.hpp"

#include "Bee/ShaderPipeline/ShaderPipeline.hpp"
#include "Bee/ShaderPipeline/Compiler.hpp"

struct ShaderFile
{
    bee::DynamicArray<bee::String>  shaders;
};

struct Vertex
{
    bee::float4 position;
    bee::float4 color;
};

static bee::GpuBackend*                                 g_gpu = nullptr;
static bee::PlatformModule*                             g_platform = nullptr;
static bee::ShaderPipelineModule*                       g_shader_pipeline = nullptr;
static bee::ShaderCompilerModule*                       g_shader_compiler = nullptr;

static bee::WindowHandle                                g_window;
static bee::DeviceHandle                                g_device;
static bee::SwapchainHandle                             g_swapchain;
static bee::RenderPassHandle                            g_pass;
static bee::BufferHandle                                g_vertex_buffer;

static bee::DynamicHashMap<bee::String, bee::Shader>    g_shader_cache;
static bee::DynamicHashMap<bee::Path, ShaderFile>       g_shader_file_to_shader;
static bee::fs::DirectoryWatcher                        g_shader_watcher;
static bee::Path                                        g_shader_root;


static bool init_platform()
{
    g_platform = static_cast<bee::PlatformModule*>(bee::get_module(BEE_PLATFORM_MODULE_NAME));

    if (BEE_FAIL(g_platform->start("Bee.RenderTests")))
    {
        return false;
    }

    bee::WindowCreateInfo info{};
    info.title = "Bee.RenderTests";
    g_window = g_platform->create_window(info);

    return g_window.is_valid();
}

static void destroy_platform()
{
    g_platform->destroy_window(g_window);
    g_platform->shutdown();
}

static bool init_gpu()
{
    auto* gpu_module = static_cast<bee::GpuModule*>(bee::get_module(BEE_GPU_MODULE_NAME));
    g_gpu = gpu_module->get_default_backend(bee::GpuApi::vulkan);
    if (BEE_FAIL(g_gpu->init()))
    {
        g_gpu = nullptr;
        return false;
    }

    int physical_device_count = g_gpu->enumerate_physical_devices(nullptr, 0);
    auto* physical_devices = BEE_ALLOCA_ARRAY(bee::PhysicalDeviceInfo, physical_device_count);
    g_gpu->enumerate_physical_devices(physical_devices, physical_device_count);

    // Log physical device list for debugging
    {
        bee::String device_list;
        bee::io::StringStream stream(&device_list);
        for (int i = 0; i < physical_device_count; ++i)
        {
            auto& pd = physical_devices[i];
            stream.write_fmt(
                "[%d] %s\n    API: %s\n    Vendor: %s\n    Type: %s",
                pd.id,
                pd.name,
                pd.api_version,
                bee::gpu_vendor_string(pd.vendor),
                bee::gpu_type_string(pd.type)
            );
        }

        bee::log_debug("Enumerating physical devices...\n%s", device_list.c_str());
    }

    // Create device and swapchain
    {
        bee::DeviceCreateInfo device_info{};
        device_info.physical_device_id = 0;
        g_device = g_gpu->create_device(device_info);
        if (BEE_FAIL(g_device.is_valid()))
        {
            return false;
        }

        bee::SwapchainCreateInfo swapchain_info{};
        const auto window_size = g_platform->get_framebuffer_size(g_window);
        swapchain_info.texture_format = bee::PixelFormat::bgra8;
        swapchain_info.texture_extent = bee::Extent(window_size.x, window_size.y);
        swapchain_info.texture_usage = bee::TextureUsage::color_attachment;
        swapchain_info.vsync = true;
        swapchain_info.window = g_window;
        swapchain_info.debug_name = "Bee.RenderTests.Swapchain";

        g_swapchain = g_gpu->create_swapchain(g_device, swapchain_info);
        if (BEE_FAIL(g_swapchain.is_valid()))
        {
            return false;
        }
    }

    // Create render pass
    {
        bee::SubPassDescriptor subpass{};
        subpass.color_attachments.size = 1;
        subpass.color_attachments[0] = 0;

        bee::RenderPassCreateInfo rp_info{};
        rp_info.attachments.size = 1;
        rp_info.attachments[0].type = bee::AttachmentType::present;
        rp_info.attachments[0].format = g_gpu->get_swapchain_texture_format(g_device, g_swapchain);
        rp_info.attachments[0].load_op = bee::LoadOp::clear;
        rp_info.attachments[0].store_op = bee::StoreOp::store;
        rp_info.subpass_count = 1;
        rp_info.subpasses = &subpass;
        g_pass = g_gpu->create_render_pass(g_device, rp_info);

        if (BEE_FAIL(g_pass.is_valid()))
        {
            return false;
        }
    }

    // Create dynamic vertex buffer
    {
        bee::BufferCreateInfo info{};
        info.size = sizeof(Vertex) * 3;
        info.type = bee::BufferType::vertex_buffer | bee::BufferType::dynamic_buffer;
        info.memory_usage = bee::DeviceMemoryUsage::cpu_to_gpu;
        info.debug_name = "Bee.RenderTests.VertexBuffer";
        g_vertex_buffer = g_gpu->create_buffer(g_device, info);
    }

    return true;
}

static void destroy_gpu()
{
    if (g_gpu == nullptr)
    {
        return;
    }

    g_gpu->submissions_wait(g_device);

    if (g_vertex_buffer.is_valid())
    {
        g_gpu->destroy_buffer(g_device, g_vertex_buffer);
    }

    if (g_pass.is_valid())
    {
        g_gpu->destroy_render_pass(g_device, g_pass);
    }

    if (g_swapchain.is_valid())
    {
        g_gpu->destroy_swapchain(g_device, g_swapchain);
    }

    if (g_device.is_valid())
    {
        g_gpu->destroy_device(g_device);
    }

    g_gpu->destroy();
    g_gpu = nullptr;
}

static bool load_shader(const bee::Path& path)
{
    auto* file_to_shader = g_shader_file_to_shader.find(path);
    if (file_to_shader == nullptr)
    {
        file_to_shader = g_shader_file_to_shader.insert(path, ShaderFile{});
    }

    auto contents = bee::fs::read(path, bee::temp_allocator());
    bee::DynamicArray<bee::Shader> results(bee::temp_allocator());

    auto res = g_shader_compiler->compile_shader(
        path.view(),
        contents.view(),
        bee::ShaderTarget::spirv,
        &results,
        bee::system_allocator()
    );

    if (!res)
    {
        bee::log_error("%s", res.unwrap_error().to_string());
        return false;
    }

    for (int i = 0; i < results.size(); ++i)
    {
        if (!g_shader_pipeline->load_shader(&results[i]))
        {
            return false;
        }

        const auto name = bee::str::format("%" BEE_PRIsv ".%s", BEE_FMT_SV(path.stem()), results[i].name.c_str());
        auto* existing = g_shader_cache.find(name);

        bool reload = false;
        if (existing != nullptr)
        {
            g_shader_pipeline->unload_shader(&existing->value);
            existing->value = BEE_MOVE(results[i]);
        }
        else
        {
            existing = g_shader_cache.insert(BEE_MOVE(name), BEE_MOVE(results[i]));
            file_to_shader->value.shaders.push_back(existing->key);
        }

        bee::log_info("%s shader: %s", reload ? "Reloaded" : "Loaded", existing->key.c_str());
    }

    return true;
}

static bool init_shaders()
{
    g_shader_compiler = static_cast<bee::ShaderCompilerModule*>(bee::get_module(BEE_SHADER_COMPILER_MODULE_NAME));
    g_shader_pipeline = static_cast<bee::ShaderPipelineModule*>(bee::get_module(BEE_SHADER_PIPELINE_MODULE_NAME));

    g_shader_pipeline->init(nullptr, g_gpu, g_device);
    g_shader_root = bee::fs::roots().installation.join("Tests/Render/Shaders");
    g_shader_watcher.add_directory(g_shader_root);
    g_shader_watcher.start("Bee.Test.Render.Watcher");

    for (const auto& file : bee::fs::read_dir(g_shader_root))
    {
        if (file.extension() == ".bsc")
        {
            if (!load_shader(file))
            {
                return false;
            }
        }
    }

    return true;
}

static void destroy_shaders()
{
    g_shader_watcher.stop();

    for (auto& shader : g_shader_cache)
    {
        g_shader_pipeline->unload_shader(&shader.value);
    }
    g_shader_cache.clear();
    g_shader_file_to_shader.clear();
    g_shader_pipeline->shutdown();
}

int bee_main(int argc, char** argv)
{
    bee::JobSystemInitInfo job_system_info{};
    bee::job_system_init(job_system_info);

    bee::init_plugins();

    add_plugin_search_path(bee::fs::roots().binaries.join("Plugins"));
    add_plugin_source_path(bee::fs::roots().sources);

    bee::refresh_plugins();
    bee::load_plugin("Bee.ShaderPipeline");
    bee::load_plugin("Bee.VulkanBackend");

    if (!init_platform())
    {
        return EXIT_FAILURE;
    }

    if (!init_gpu())
    {
        destroy_platform();
        return EXIT_FAILURE;
    }

    if (!init_shaders())
    {
        destroy_shaders();
        return EXIT_FAILURE;
    }

    bee::DynamicArray<bee::fs::FileNotifyInfo> shader_events;
    const bee::u64 start_time = bee::time::now();

    while (!g_platform->window_close_requested(g_window))
    {
        bee::temp_allocator_reset();

        bee::refresh_plugins();

        g_shader_watcher.pop_events(&shader_events);
        for (const auto& event : shader_events)
        {
            if (event.file.extension() != ".bsc")
            {
                continue;
            }

            switch (event.action)
            {
                case bee::fs::FileAction::added:
                case bee::fs::FileAction::modified:
                {
                    load_shader(event.file);
                    break;
                }
                case bee::fs::FileAction::removed:
                {
                    auto* file_to_shader = g_shader_file_to_shader.find(event.file);
                    if (file_to_shader == nullptr)
                    {
                        break;
                    }
                    for (const auto& name : file_to_shader->value.shaders)
                    {
                        auto* shader = g_shader_cache.find(name);
                        if (shader == nullptr)
                        {
                            continue;
                        }

                        g_shader_pipeline->unload_shader(&shader->value);
                        g_shader_cache.erase(name);
                    }
                    g_shader_file_to_shader.erase(event.file);
                    break;
                }
                default: break;
            }
        }

        g_platform->poll_input();

        Vertex vertices[] = {
            { bee::float4( 0.0f, -0.5f, 0.0f, 1.0f), bee::float4(1.0f, 0.0f, 0.0f, 1.0f) },
            { bee::float4( 0.5f,  0.5f, 0.0f, 1.0f), bee::float4(0.0f, 1.0f, 0.0f, 1.0f) },
            { bee::float4(-0.5f,  0.5f, 0.0f, 1.0f), bee::float4(0.0f, 0.0f, 1.0f, 1.0f) }
        };
        g_gpu->update_buffer(g_device, g_vertex_buffer, vertices, 0, sizeof(Vertex) * 3);

        auto* shader = g_shader_cache.find("Triangle.TrianglePipeline");
        if (shader != nullptr)
        {
            auto* cmd = g_gpu->get_command_backend();
            auto* cmdbuf = g_gpu->allocate_command_buffer(g_device, bee::QueueType::graphics);
            cmd->begin(cmdbuf, bee::CommandBufferUsage::submit_once);
            {
                struct PushConstant
                {
                    float time { 0.0f };
                } push_constant;
                push_constant.time = bee::sign_cast<float>(bee::time::total_seconds(bee::time::now() - start_time));
                cmd->push_constants(cmdbuf, 0, &push_constant);

                const auto backbuffer = g_gpu->get_swapchain_texture_view(g_device, g_swapchain);
                const auto swapchain_extent = g_gpu->get_swapchain_extent(g_device, g_swapchain);
                const auto backbuffer_rect = bee::RenderRect (0, 0, swapchain_extent.width, swapchain_extent.height);
                bee::ClearValue clear_value(0.3f, 0.3f, 0.3f, 1.0f);

                // scissor and viewport are dynamic states by default so need to be set each frame
                cmd->set_scissor(cmdbuf, backbuffer_rect);
                cmd->set_viewport(cmdbuf, bee::Viewport(0, 0, static_cast<float>(backbuffer_rect.width), static_cast<float>(backbuffer_rect.height)));
                cmd->begin_render_pass(
                    cmdbuf,
                    g_pass,
                    1, &backbuffer,
                    backbuffer_rect,
                    1, &clear_value
                );
                cmd->bind_vertex_buffer(cmdbuf, g_vertex_buffer, 0, 0);
                cmd->draw(cmdbuf, shader->value.pipeline_desc, 3, 1, 0, 0);
                cmd->end_render_pass(cmdbuf);
            }
            cmd->end(cmdbuf);

            bee::SubmitInfo submit{};
            submit.command_buffer_count = 1;
            submit.command_buffers = &cmdbuf;
            g_gpu->submit(g_device, submit);

            g_gpu->present(g_device, g_swapchain);
        }

        g_gpu->commit_frame(g_device);
    }

    destroy_shaders();
    destroy_gpu();
    destroy_platform();

    bee::job_system_shutdown();
    bee::shutdown_plugins();
    return 0;
}

