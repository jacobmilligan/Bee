/*
 *  Loader.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/Plugins/ShaderPipeline/Shader.hpp"
#include "Bee/Plugins/AssetRegistry/AssetRegistry.hpp"

namespace bee {


void get_supported_shader_types(DynamicArray<TypeRef>* types)
{
    types->push_back(get_type<Shader>());
}

TypeRef get_parameter_type()
{
    return get_type<DeviceHandle>();
}

void* allocate_shader(const TypeRef& type)
{
    const auto shader_type = get_type<Shader>();
    BEE_ASSERT(type == shader_type);
    return BEE_NEW(system_allocator(), Shader);
}

AssetStatus load_shader(AssetLoaderContext* ctx, io::Stream* stream)
{
    auto* shader = ctx->get_asset<Shader>();
    shader->gpu_device = *ctx->get_parameter<DeviceHandle>();

    StreamSerializer serializer(stream);
    serialize(SerializerMode::reading, &serializer, shader);

    // init all the passes
    RenderPassCreateInfo pass_info{};
    for (auto& pass : shader->passes)
    {
        if (pass.gpu_handle.is_valid())
        {
            gpu_destroy_render_pass(shader->gpu_device, pass.gpu_handle);
        }

        pass_info.subpass_count = pass.subpasses.size;
        pass_info.subpasses = shader->subpasses.data() + pass.subpasses.offset;
        pass_info.attachment_count = pass.attachments.size;
        const auto attachments_bytes = sizeof(AttachmentDescriptor) * pass.attachments.size;
        memcpy(pass_info.attachments, shader->attachments.data() + pass.attachments.offset, attachments_bytes);

        pass.gpu_handle = gpu_create_render_pass(shader->gpu_device, pass_info);
    }

    // Create the shaders before the pipeline
    ShaderCreateInfo shader_info{};
    for (auto& subshader : shader->subshaders)
    {
        for (int stage_index = 0; stage_index < static_array_length(subshader.stage_handles); ++stage_index)
        {
            auto& code_range = subshader.stage_code_ranges[stage_index];

            if (code_range.empty())
            {
                continue;
            }

            shader_info.entry = subshader.stage_entries[stage_index].c_str();
            shader_info.code = shader->code.data() + code_range.offset;
            shader_info.code_size = code_range.size;

            if (subshader.stage_handles[stage_index].is_valid())
            {
                gpu_destroy_shader(shader->gpu_device, subshader.stage_handles[stage_index]);
            }

            subshader.stage_handles[stage_index] = gpu_create_shader(shader->gpu_device, shader_info);
        }
    }

    // Create the pipeline using the resources we just created
    for (int pipeline_index = 0; pipeline_index < shader->pipelines.size(); ++pipeline_index)
    {
        auto& pipeline = shader->pipelines[pipeline_index];

        if (pipeline.gpu_handle.is_valid())
        {
            gpu_destroy_pipeline_state(shader->gpu_device, pipeline.gpu_handle);
        }

        auto& info = pipeline.info;

        if (pipeline.pass < shader->passes.size())
        {
            return AssetStatus::loading_failed;
        }

        info.compatible_render_pass = shader->passes[pipeline.pass].gpu_handle;

        info.vertex_stage = shader->get_shader(pipeline_index, ShaderStageIndex::vertex);
        info.fragment_stage = shader->get_shader(pipeline_index, ShaderStageIndex::fragment);

        if (!info.vertex_stage.is_valid() || info.fragment_stage.is_valid())
        {
            return AssetStatus::loading_failed;
        }

        pipeline.gpu_handle = gpu_create_pipeline_state(shader->gpu_device, info);
    }

    return AssetStatus::loaded;
}

AssetStatus unload_shader(AssetLoaderContext* ctx)
{
    auto* shader = ctx->get_asset<Shader>();

    for (auto& pass : shader->passes)
    {
        if (pass.gpu_handle.is_valid())
        {
            gpu_destroy_render_pass(shader->gpu_device, pass.gpu_handle);
        }
    }

    ShaderCreateInfo shader_info{};
    for (auto& subshader : shader->subshaders)
    {
        for (auto& gpu_handle : subshader.stage_handles)
        {
            if (gpu_handle.is_valid())
            {
                gpu_destroy_shader(shader->gpu_device, gpu_handle);
            }
        }
    }

    for (auto& pipeline : shader->pipelines)
    {
        if (pipeline.gpu_handle.is_valid())
        {
            gpu_destroy_pipeline_state(shader->gpu_device, pipeline.gpu_handle);
        }
    }

    BEE_DELETE(system_allocator(), shader);

    return AssetStatus::unloaded;
}


static AssetLoader g_loader{};

void load_asset_loader(bee::PluginRegistry* registry, const bee::PluginState state)
{
    if (!registry->has_module(BEE_ASSET_REGISTRY_MODULE_NAME))
    {
        return;
    }

    g_loader.get_supported_types = get_supported_shader_types;
    g_loader.get_parameter_type = get_parameter_type;
    g_loader.allocate = allocate_shader;
    g_loader.load = load_shader;
    g_loader.unload = unload_shader;

    auto* asset_registry = registry->get_module<AssetRegistryModule>(BEE_ASSET_REGISTRY_MODULE_NAME);
    if (asset_registry->add_loader != nullptr)
    {
        asset_registry->add_loader(&g_loader);
    }
}


} // namespace bee