/*
 *  Shader.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/ShaderPipeline/Resource.inl"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"


namespace bee {


struct GlobalData
{
    AssetLoader loader;
};

static GlobalData*          g_global = nullptr;
static AssetCacheModule*    g_asset_cache = nullptr;

/*
 **********************************
 *
 * Shader cache implementation
 *
 **********************************
 */
void unload_shader(ShaderCache* cache, ShaderPipeline* shader)
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
        unload_shader(cache, &shader.resource);
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

void register_asset_loader(ShaderCache* shader_cache, AssetCache* asset_cache, const GpuBackend* gpu, const DeviceHandle device)
{
    BEE_ASSERT(shader_cache->gpu == nullptr);
    BEE_ASSERT(!shader_cache->device.is_valid());

    if (g_asset_cache == nullptr)
    {
        g_asset_cache = static_cast<AssetCacheModule*>(get_module(BEE_ASSET_CACHE_MODULE_NAME));
    }

    shader_cache->gpu = gpu;
    shader_cache->device = device;
    g_asset_cache->register_loader(asset_cache, &g_global->loader, shader_cache);
}

void unregister_asset_loader(ShaderCache* shader_cache, AssetCache* asset_cache)
{
    BEE_ASSERT(g_asset_cache != nullptr);
    g_asset_cache->unregister_loader(asset_cache, &g_global->loader);
}

ShaderPipelineHandle lookup_shader(ShaderCache* cache, const u32 name_hash)
{
    scoped_recursive_lock_t lock(cache->mutex);
    auto* handle = cache->lookup.find(name_hash);
    return handle == nullptr ? ShaderPipelineHandle{} : handle->value;
}

u32 get_shader_hash(ShaderCache* cache, const ShaderPipelineHandle handle)
{
    scoped_recursive_lock_t lock(cache->mutex);
    return cache->pool[handle].name_hash;
}

/*
 ********************************************
 *
 * Runtime Asset loader
 *
 ********************************************
 */

static i32 get_shader_loader_types(Type* dst)
{
    if (dst != nullptr)
    {
        dst[0] = get_type<ShaderPipeline>();
    }
    return 1;
}

static bool load_shader_pipeline(ShaderCache* cache, const AssetStreamInfo& stream_info)
{
    const auto hash = get_hash(stream_info.hash);
    auto handle = lookup_shader(cache, hash);
    if (!handle.is_valid())
    {
        handle = cache->pool.allocate();
        cache->lookup.insert(hash, handle);
    }
    auto& shader = cache->pool[handle];

    for (const auto& stage : shader.stages)
    {
        if (stage.shader_resource.is_valid())
        {
            cache->gpu->destroy_shader(cache->device, stage.shader_resource);
        }
    }

    io::FileStream stream(stream_info.path, "rb");
    StreamSerializer serializer(&stream);
    serialize(SerializerMode::reading, &serializer, &shader, temp_allocator());

    ShaderCreateInfo info{};
    for (int stage_index = 0; stage_index < shader.stages.size; ++stage_index)
    {
        auto& stage = shader.stages[stage_index];

        info.code = stage.code.data();
        info.code_size = stage.code.size();
        info.entry = stage.entry.data();
        stage.shader_resource = cache->gpu->create_shader(cache->device, info);
        BEE_ASSERT(stage.shader_resource.is_valid());

        switch (static_cast<ShaderStageIndex>(stage_index))
        {
            case ShaderStageIndex::vertex:
            {
                shader.pipeline_desc.vertex_stage = stage.shader_resource;
                break;
            }
            case ShaderStageIndex::fragment:
            {
                shader.pipeline_desc.fragment_stage = stage.shader_resource;
                break;
            }
            default:
            {
                BEE_UNREACHABLE("Unsupported shader stage");
            }
        }
    }

    return true;
}

Result<void*, AssetCacheError> load_shader(const AssetLocation* location, void* user_data)
{
    auto* cache = static_cast<ShaderCache*>(user_data);
    auto* asset = BEE_NEW(system_allocator(), ShaderAsset);

    asset->pipelines.resize(location->streams.size);

    for (int i = 0; i < location->streams.size; ++i)
    {
        load_shader_pipeline(cache, location->streams[i]);
    }

    return asset;
}

bool unload_shader(const Type type, void* data, void* user_data)
{
    auto* asset = static_cast<ShaderAsset*>(data);
    auto* cache = static_cast<ShaderCache*>(user_data);

    for (int i = 0; i < asset->pipelines.size(); ++i)
    {
        auto& pipeline = cache->pool[asset->pipelines[i]];
        unload_shader(cache, &pipeline);
    }

    BEE_DELETE(system_allocator(), asset);
    return true;
}

/*
 **********************************
 *
 * Shader implementation
 *
 **********************************
 */
RenderPassHandle get_render_pass(const ShaderPipelineHandle shader)
{
    return RenderPassHandle{};
}

PipelineStateDescriptor get_pipeline_state(const ShaderPipelineHandle shader)
{
    return PipelineStateDescriptor{};
}

ShaderHandle get_stage(const ShaderPipelineHandle, const ShaderStageIndex stage)
{
    return ShaderHandle{};
}

void load(const ShaderPipelineHandle handle)
{

}

void unload(const ShaderPipelineHandle handle)
{

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

    g_global->loader.get_types = get_shader_loader_types;
    g_global->loader.load = load_shader;
    g_global->loader.unload = unload_shader;

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