/*
 *  main.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include <Bee/Application/Main.hpp>
#include <Bee/Graphics/GPU.hpp>
#include <Bee/AssetPipeline/AssetPipeline.hpp>
#include <Bee/Core/Filesystem.hpp>
#include <Bee/Asset/Asset.hpp>
#include <Bee/ShaderCompiler/Compile.hpp>
#include <Bee/Graphics/Shader.hpp>


class CubesApp final : public bee::Application
{
public:
    int launch(bee::AppContext* ctx) override
    {
        bee::job_system_init(bee::JobSystemInitInfo{});

        bee::AssetPipelineInitInfo pipeline_info{};
        pipeline_info.asset_source_root = bee::fs::get_root_dirs().assets_root.c_str();
        pipeline_info.assetdb_name = "AssetDB";
        pipeline_info.assetdb_location = bee::fs::get_root_dirs().data_root.c_str();
        pipeline_.init(pipeline_info);
        pipeline_.register_asset_compiler<bee::ShaderCompiler>();

        bee::AssetCompileRequest req("Shaders/BSCTestShader.bsc", bee::asset_platform_default(), bee::ShaderCompilerSettings{true});

        bee::JobGroup group{};
        pipeline_.import_assets(&group, 1, &req);
        bee::job_wait(&group);

        bee::register_asset_loader(&shader_loader_);

        bee::PhysicalDeviceInfo devices[BEE_GPU_MAX_PHYSICAL_DEVICES];
        const auto device_count = bee::gpu_enumerate_physical_devices(devices, BEE_GPU_MAX_DEVICES);

        auto shader = bee::load_asset<bee::Shader>(bee::guid_from_string("a53b032dbc8d418dba08e3ef2010fe31"));

        bee::log_info("Enumerating available GPU's:");
        for (int d = 0; d < device_count; ++d)
        {
            bee::log_info(
                "  %s:\n  => id: %d\n  => api_version: %s\n  => vendor: %s\n  => type: %s",
                devices[d].name,
                devices[d].id,
                devices[d].api_version,
                bee::gpu_vendor_string(devices[d].vendor),
                bee::gpu_type_string(devices[d].type)
            );
        }

        bee::DeviceCreateInfo device_info{};
        device_info.physical_device_id = 0;
        device_ = bee::gpu_create_device(device_info);

        bee::SwapchainCreateInfo swapchain_info{};
        swapchain_info.texture_format = bee::PixelFormat::bgra8;
        swapchain_info.texture_extent = bee::Extent::from_platform_size(bee::get_window_size(ctx->main_window));
        swapchain_info.texture_usage = bee::TextureUsage::color_attachment;
        swapchain_info.texture_array_layers = 1;
        swapchain_info.vsync = true;
        swapchain_info.window = ctx->main_window;
        swapchain_info.debug_name = "Main swapchain";
        swapchain_ = bee::gpu_create_swapchain(device_, swapchain_info);

        return 0;
    }

    void shutdown(bee::AppContext* ctx) override
    {
        bee::gpu_destroy_swapchain(device_, swapchain_);
        bee::gpu_destroy_device(device_);
    }

    void tick(bee::AppContext* ctx) override
    {
        if (bee::key_typed(ctx->default_input, bee::Key::escape))
        {
            ctx->quit = true;
        }
    }

private:
    bee::DeviceHandle       device_;
    bee::SwapchainHandle    swapchain_;
    bee::AssetPipeline      pipeline_;
    bee::ShaderLoader       shader_loader_;
};

int bee_main(int argc, char** argv)
{
    CubesApp app;
    bee::AppLaunchConfig config{};
    return bee::app_loop(config, &app);
}