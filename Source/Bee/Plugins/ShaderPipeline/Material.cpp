/*
 *  Material.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Serialization/BinarySerializer.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"

#include "Bee/Plugins/ShaderPipeline/Material.hpp"
#include "Bee/Plugins/ShaderPipeline/Compiler.hpp"
#include "Bee/Plugins/AssetPipeline/AssetPipeline.hpp"


namespace bee {


static AssetRegistryModule* g_asset_registry { nullptr };

/*
 ****************************
 *
 * Material asset compiler
 *
 ****************************
 */
const char* get_material_compiler_name()
{
    return "Bee Material Compiler";
}

i32 supported_material_file_types(const char** filetypes)
{
    if (filetypes != nullptr)
    {
        filetypes[0] = ".mat";
    }
    return 1;
}

AssetCompilerOrder get_material_compiler_order()
{
    return material_compiler_order;
}

AssetCompilerStatus compile(AssetCompilerData* data, const i32 thread_index, AssetCompilerContext* ctx)
{
    auto contents = fs::read(ctx->location(), ctx->temp_allocator());

    MaterialFile file(ctx->temp_allocator());

    JSONSerializer serializer(contents.data(), rapidjson::kParseInsituFlag, ctx->temp_allocator());
    serialize(SerializerMode::reading, &serializer, &file, ctx->temp_allocator());

    if (file.shader.empty())
    {
        return AssetCompilerStatus::invalid_source_format;
    }

    GUID shader_guid{};
    if (!ctx->uri_to_guid(file.shader.view(), &shader_guid))
    {
        return AssetCompilerStatus::fatal_error;
    }

    Material material{};
    material.shader = Asset<Shader>(shader_guid);

    if (!material.shader.load(g_asset_registry, DeviceHandle{}))
    {
        return AssetCompilerStatus::fatal_error;
    }

    const auto pipeline_index = find_index_if(material.shader->pipelines, [&](const Shader::Pipeline& pipeline)
    {
        return pipeline.name.view() == file.pipeline.view();
    });

    if (pipeline_index < 0)
    {
        return AssetCompilerStatus::fatal_error;
    }

    material.pipeline = pipeline_index;

    ctx->add_dependency(shader_guid);

    auto& artifact = ctx->add_artifact<Material>();
    {
        BinarySerializer binary_serializer(&artifact);
        serialize(SerializerMode::writing, &binary_serializer, &material);
    }
    ctx->set_main(artifact);

    return AssetCompilerStatus::success;
}

static AssetCompiler g_material_compiler{};

void load_material_compiler(PluginRegistry* registry, const PluginState state)
{
    g_asset_registry = registry->get_module<AssetRegistryModule>(BEE_ASSET_REGISTRY_MODULE_NAME);
    auto* asset_pipeline = registry->get_module<AssetPipelineModule>(BEE_ASSET_PIPELINE_MODULE_NAME);

    g_material_compiler.get_name = get_material_compiler_name;
    g_material_compiler.supported_file_types = supported_material_file_types;
    g_material_compiler.compile = compile;
    g_material_compiler.get_order = get_material_compiler_order;

    if (state == PluginState::loading)
    {
        asset_pipeline->register_compiler(&g_material_compiler);
    }
    else
    {
        asset_pipeline->unregister_compiler(&g_material_compiler);
    }
}

/*
 **********************************
 *
 * Material runtime asset loader
 *
 **********************************
 */
static constexpr size_t g_material_chunk_size = sizeof(Material) * 16;
static PoolAllocator* g_material_allocator;


i32 get_supported_material_type(TypeRef* types)
{
    if (types != nullptr)
    {
        types[0] = get_type<Material>();
    }
    return 1;
}

TypeRef get_material_parameter_type()
{
    return get_type<DeviceHandle>();
}

void* allocate_material(const TypeRef& type)
{
    return BEE_NEW(g_material_allocator, Material);
}

AssetStatus load_material(AssetLoaderContext* ctx, const i32 stream_count, const TypeRef* stream_types, io::Stream** streams)
{
    auto* material = ctx->get_asset<Material>();
    const auto* device = ctx->get_arg<DeviceHandle>();

    StreamSerializer serializer(streams[0]);
    serialize(SerializerMode::reading, &serializer, material);

    if (!material->shader.load(ctx->registry(), *device))
    {
        return AssetStatus::loading_failed;
    }

    return AssetStatus::loaded;
}

AssetStatus unload_material(AssetLoaderContext* ctx)
{
    BEE_DELETE(g_material_allocator, ctx->get_asset<Material>());
    return AssetStatus::unloaded;
}


static AssetLoader      g_loader{};

void load_material_loader(PluginRegistry* registry, const PluginState state)
{
    g_material_allocator = registry->get_or_create_persistent<PoolAllocator>("BeeMaterialAllocator", g_material_chunk_size, 64, 0);

    g_loader.get_supported_types = get_supported_material_type;
    g_loader.get_parameter_type = get_material_parameter_type;
    g_loader.allocate = allocate_material;
    g_loader.load = load_material;
    g_loader.unload = unload_material;

    auto* asset_reg = registry->get_module<AssetRegistryModule>(BEE_ASSET_REGISTRY_MODULE_NAME);

    if (state == PluginState::loading)
    {
        asset_reg->add_loader(&g_loader);
    }
    else
    {
        asset_reg->remove_loader(&g_loader);
    }
}


} // namespace bee