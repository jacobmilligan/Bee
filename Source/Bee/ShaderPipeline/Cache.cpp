/*
 *  Shader.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/ShaderPipeline/Resource.inl"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"


namespace bee {


struct GlobalData
{
    AssetLoader loader;
};

static GlobalData*          g_global = nullptr;
static AssetPipelineModule* g_asset_pipeline = nullptr;

/*
 **********************************
 *
 * Shader cache implementation
 *
 **********************************
 */
void unload_pipeline(ShaderCache* cache, ShaderPipeline* shader)
{
    for (const auto& stage : shader->stages)
    {
        if (stage.shader_resource.is_valid())
        {
            cache->gpu->destroy_shader(cache->device, stage.shader_resource);
        }
    }
}

ShaderCache* create()
{
    return BEE_NEW(system_allocator(), ShaderCache);
}

void destroy(ShaderCache* cache)
{
    for (auto shader : cache->pool)
    {
        unload_pipeline(cache, &shader.resource);
    }

    BEE_DELETE(system_allocator(), cache);
}

u32 get_shader_name_hash(const StringView& name)
{
    // should match static_string_hash
    return detail::runtime_fnv1a(name.c_str(), name.size());
}

void load_cache(ShaderCache* cache, Serializer* serializer)
{
    scoped_recursive_lock_t lock(cache->mutex);
    serialize(SerializerMode::reading, serializer, &cache->pool);

    for (auto shader : cache->pool)
    {
        cache->lookup.insert(shader.resource.name_hash, shader.handle);
    }
}

void save_cache(ShaderCache* cache, Serializer* serializer)
{
    scoped_recursive_lock_t lock(cache->mutex);
    serialize(SerializerMode::writing, serializer, &cache->pool);
}

ShaderPipelineHandle add_shader(ShaderCache* cache, const ShaderPipelineDescriptor& desc)
{
    scoped_recursive_lock_t lock(cache->mutex);

    const u32 hash = get_shader_name_hash(desc.name);
    auto* existing = cache->lookup.find(hash);
    ShaderPipelineHandle handle{};

    // We can either re-add an existing shader or create a new one here
    if (existing == nullptr)
    {
        handle = cache->pool.allocate();
        cache->lookup.insert(hash, handle);
    }
    else
    {
        handle = existing->value;
    }

    auto& pipeline = cache->pool[handle];

    // Pipeline source path, name, and info
    pipeline.name_hash = get_shader_name_hash(desc.name);
    pipeline.pipeline_desc = desc.pipeline;

    // Shader stages
    pipeline.stages.size = desc.shader_stage_count;
    for (int i = 0; i < desc.shader_stage_count; ++i)
    {
        auto& stage = pipeline.stages[i];
        stage.entry.assign(desc.shader_info[i].entry);
        stage.code.resize(desc.shader_info[i].code_size);
        stage.code.copy(0, desc.shader_info[i].code, desc.shader_info[i].code + desc.shader_info[i].code_size);
        stage.flags = desc.shader_stages[i];
        stage.update_frequencies.size = desc.shader_resources[i].layout_count;
        memcpy(stage.update_frequencies.data, desc.shader_resources[i].frequencies, sizeof(ResourceBindingUpdateFrequency) * stage.update_frequencies.size);
    }

    return handle;
}

void remove_shader(ShaderCache* cache, const ShaderPipelineHandle handle)
{
    scoped_recursive_lock_t lock(cache->mutex);

    if (BEE_FAIL(cache->pool.is_active(handle)))
    {
        return;
    }

    auto& shader = cache->pool[handle];
    cache->lookup.erase(shader.name_hash);
    cache->pool.deallocate(handle);
}

void register_asset_loader(ShaderCache* shader_cache, AssetPipeline* asset_pipeline, const GpuBackend* gpu, const DeviceHandle device)
{
    BEE_ASSERT(shader_cache->gpu == nullptr);
    BEE_ASSERT(!shader_cache->device.is_valid());

    if (g_asset_pipeline == nullptr)
    {
        g_asset_pipeline = static_cast<AssetPipelineModule*>(get_module(BEE_ASSET_PIPELINE_MODULE_NAME));
    }

    shader_cache->gpu = gpu;
    shader_cache->device = device;
    g_asset_pipeline->register_loader(asset_pipeline, &g_global->loader, shader_cache);
}

void unregister_asset_loader(ShaderCache* shader_cache, AssetPipeline* asset_pipeline)
{
    BEE_ASSERT(g_asset_pipeline != nullptr);
    g_asset_pipeline->unregister_loader(asset_pipeline, &g_global->loader);
}

ShaderPipelineHandle lookup_shader(ShaderCache* cache, const u32 name_hash)
{
    scoped_recursive_lock_t lock(cache->mutex);
    auto* handle = cache->lookup.find(name_hash);
    if (handle == nullptr)
    {
        return {};
    }

    return handle == nullptr ? ShaderPipelineHandle{} : handle->value;
}

u32 get_shader_hash(ShaderCache* cache, const ShaderPipelineHandle handle)
{
    scoped_recursive_lock_t lock(cache->mutex);
    return cache->pool[handle].name_hash;
}

bool load_shader(ShaderCache* cache, const ShaderPipelineHandle handle)
{
    if (!cache->pool.is_active(handle))
    {
        return false;
    }

    auto& pipeline = cache->pool[handle];

    ShaderCreateInfo info{};
    for (int stage_index = 0; stage_index < pipeline.stages.size; ++stage_index)
    {
        auto& stage = pipeline.stages[stage_index];
        if (stage.shader_resource.is_valid())
        {
            cache->gpu->destroy_shader(cache->device, stage.shader_resource);
        }

        info.code = stage.code.data();
        info.code_size = stage.code.size();
        info.entry = stage.entry.data();

        switch (static_cast<ShaderStageIndex>(stage_index))
        {
            case ShaderStageIndex::vertex:
            {
                pipeline.pipeline_desc.vertex_stage = stage.shader_resource;
                break;
            }
            case ShaderStageIndex::fragment:
            {
                pipeline.pipeline_desc.fragment_stage = stage.shader_resource;
                break;
            }
            default:
            {
                return false;
            }
        }

        stage.shader_resource = cache->gpu->create_shader(cache->device, info);
        BEE_ASSERT(stage.shader_resource.is_valid());
    }

    ResourceBindingCreateInfo binding_info{};
    for (u32 layout_index = 0; layout_index < pipeline.pipeline_desc.resource_layouts.size; ++layout_index)
    {
        auto& layout = pipeline.pipeline_desc.resource_layouts[layout_index];
        if (pipeline.resource_bindings[layout_index].is_valid())
        {
            cache->gpu->destroy_resource_binding(cache->device, pipeline.resource_bindings[layout_index]);
        }

        binding_info.layout = &layout;
        pipeline.resource_bindings[layout_index] = cache->gpu->create_resource_binding(cache->device, binding_info);
    }


    return true;
}

bool unload_shader(ShaderCache* cache, const ShaderPipelineHandle handle)
{
    if (!cache->pool.is_active(handle))
    {
        return false;
    }

    auto& pipeline = cache->pool[handle];
    unload_pipeline(cache, &pipeline);
    return true;
}

const PipelineStateDescriptor& get_pipeline_desc(ShaderCache* cache, const ShaderPipelineHandle handle)
{
    return cache->pool[handle].pipeline_desc;
}

void update_resources(ShaderCache* cache, const ShaderPipelineHandle handle, const u32 layout, const i32 count, const ResourceBindingUpdate* updates)
{
    auto& pipeline = cache->pool[handle];
    const auto binding = pipeline.resource_bindings[layout];
    cache->gpu->update_resource_binding(cache->device, binding, count, updates);
}

void bind_resource_layout(ShaderCache* cache, const ShaderPipelineHandle handle, CommandBuffer* cmd_buf, const u32 layout)
{
    auto& pipeline = cache->pool[handle];
    auto* cmd = cache->gpu->get_command_backend();
    cmd->bind_resources(cmd_buf, layout, pipeline.resource_bindings[layout]);
}

void bind_resources(ShaderCache* cache, const ShaderPipelineHandle handle, CommandBuffer* cmd_buf)
{
    auto& pipeline = cache->pool[handle];
    auto* cmd = cache->gpu->get_command_backend();
    for (int i = 0; i < pipeline.resource_bindings.size; ++i)
    {
        cmd->bind_resources(cmd_buf, i, pipeline.resource_bindings[i]);
    }
}

/*
 ********************************************
 *
 * Runtime Asset loader
 *
 ********************************************
 */

static i32 asset_cache_get_shader_types(Type* dst)
{
    if (dst != nullptr)
    {
        dst[0] = get_type<ShaderPipeline>();
    }
    return 1;
}

Result<void*, AssetPipelineError> asset_pipeline_load_shader(const GUID guid, const AssetLocation* location, void* user_data, const AssetHandle handle, void* data)
{
    auto* cache = static_cast<ShaderCache*>(user_data);
    auto* asset = BEE_NEW(system_allocator(), FixedArray<u32>);
    auto& stream_info = location->streams[0];

    if (stream_info.kind == AssetStreamKind::file)
    {
        io::FileStream stream(stream_info.path, "rb");
        stream.seek(stream_info.offset, io::SeekOrigin::begin);
        StreamSerializer serializer(&stream);
        serialize(SerializerMode::reading, &serializer, asset, temp_allocator());
    }
    else
    {
        io::MemoryStream stream(stream_info.buffer, stream_info.size);
        stream.seek(stream_info.offset, io::SeekOrigin::begin);
        StreamSerializer serializer(&stream);
        serialize(SerializerMode::reading, &serializer, asset, temp_allocator());
    }

    for (const u32 hash : *asset)
    {
        const auto shader_handle = lookup_shader(cache, hash);
        load_shader(cache, shader_handle);
    }

    return asset;
}

Result<void, AssetPipelineError> asset_pipeline_unload_shader(const Type type, void* data, void* user_data)
{
    auto* asset = static_cast<FixedArray<u32>*>(data);
    auto* cache = static_cast<ShaderCache*>(user_data);

    for (const u32 hash : *asset)
    {
        const auto handle = lookup_shader(cache, hash);
        if (!handle.is_valid())
        {
            return { AssetPipelineError::missing_data };
        }

        auto& pipeline = cache->pool[handle];
        unload_pipeline(cache, &pipeline);
    }

    BEE_DELETE(system_allocator(), asset);
    return {};
}

/*
 ********************
 *
 * Plugin loading
 *
 ********************
 */
ShaderCacheModule       g_shader_cache{}; // extern

void load_shader_modules(bee::PluginLoader* loader, const bee::PluginState state)
{
    g_global = loader->get_static<GlobalData>("Bee.RuntimeShaderData");

    g_global->loader.get_types = asset_cache_get_shader_types;
    g_global->loader.load = asset_pipeline_load_shader;
    g_global->loader.unload = asset_pipeline_unload_shader;

    // ShaderCache module
    g_shader_cache.create = create;
    g_shader_cache.destroy = destroy;
    g_shader_cache.load = load_cache;
    g_shader_cache.save = save_cache;
    g_shader_cache.add_shader = add_shader;
    g_shader_cache.remove_shader = remove_shader;
    g_shader_cache.lookup_shader = lookup_shader;
    g_shader_cache.get_shader_name_hash = get_shader_name_hash;
    g_shader_cache.get_shader_hash = get_shader_hash;
    g_shader_cache.register_asset_loader = register_asset_loader;
    g_shader_cache.unregister_asset_loader = unregister_asset_loader;

    loader->set_module(BEE_SHADER_CACHE_MODULE_NAME, &g_shader_cache, state);
}

} // namespace bee