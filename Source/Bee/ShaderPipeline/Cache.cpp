/*
 *  Shader.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/ShaderPipeline/Resource.inl"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Containers/ResourcePool.hpp"


namespace bee {


struct ShaderCache
{
    RecursiveMutex                                              mutex;
    DynamicHashMap<u32, ShaderPipelineHandle>                   lookup;
    ResourcePool<ShaderPipelineHandle, ShaderPipeline>          pool;

    ShaderCache()
        : pool(sizeof(ShaderPipeline) * 64)
    {}
};

struct RuntimeShaderData
{
    const GpuBackend*   gpu { nullptr };
    DeviceHandle        device;
};

static RuntimeShaderData* g_runtime = nullptr;

/*
 **********************************
 *
 * Shader cache implementation
 *
 **********************************
 */
void unload_shader(ShaderCache* cache, ShaderPipeline* shader);

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
        const u32 hash = get_shader_name_hash(shader.resource.name.view());
        cache->lookup.insert(hash, shader.handle);
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
    pipeline.name.assign(desc.name);
    pipeline.pipeline_info = desc.create_info;

    // Render pass and subpasses
    pipeline.subpasses.resize(desc.render_pass_info.subpass_count);
    pipeline.subpasses.copy(0, desc.render_pass_info.subpasses, desc.render_pass_info.subpasses + desc.render_pass_info.subpass_count);
    pipeline.render_pass_info = desc.render_pass_info;
    pipeline.render_pass_info.subpasses = pipeline.subpasses.data();

    // Shader stages
    pipeline.stages.resize(desc.shader_stage_count);
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
    cache->lookup.erase(get_shader_name_hash(shader.name.view()));
    cache->pool.deallocate(handle);
}

void init(const GpuBackend* gpu, const DeviceHandle device)
{
    BEE_ASSERT(g_runtime->gpu == nullptr);
    BEE_ASSERT(!g_runtime->device.is_valid());

    g_runtime->gpu = gpu;
    g_runtime->device = device;
}

ShaderPipeline* load_shader(ShaderCache* cache, const ShaderPipelineHandle handle)
{
    auto& shader = cache->pool[handle];

    ShaderCreateInfo info{};
    for (int i = 0; i < shader.stages.size(); ++i)
    {
        auto& stage = shader.stages[i];

        if (stage.shader_resource.is_valid())
        {
            g_runtime->gpu->destroy_shader(g_runtime->device, stage.shader_resource);
        }

        info.code = stage.code.data();
        info.code_size = stage.code.size();
        info.entry = stage.entry.data();
        stage.shader_resource = g_runtime->gpu->create_shader(g_runtime->device, info);
        BEE_ASSERT(stage.shader_resource.is_valid());

        switch (static_cast<ShaderStageIndex>(i))
        {
            case ShaderStageIndex::vertex:
            {
                shader.pipeline_info.vertex_stage = stage.shader_resource;
                break;
            }
            case ShaderStageIndex::fragment:
            {
                shader.pipeline_info.fragment_stage = stage.shader_resource;
                break;
            }
            default:
            {
                BEE_UNREACHABLE("Unsupported shader stage");
            }
        }
    }

    if (shader.pipeline_resource.is_valid())
    {
        g_runtime->gpu->destroy_pipeline_state(g_runtime->device, shader.pipeline_resource);
    }
    if (shader.render_pass_resource.is_valid())
    {
        g_runtime->gpu->destroy_render_pass(g_runtime->device, shader.render_pass_resource);
    }

    shader.render_pass_resource = g_runtime->gpu->create_render_pass(g_runtime->device, shader.render_pass_info);

    shader.pipeline_info.compatible_render_pass = shader.render_pass_resource;
    shader.pipeline_resource = g_runtime->gpu->create_pipeline_state(g_runtime->device, shader.pipeline_info);

    return &shader;
}

void unload_shader(ShaderCache* cache, ShaderPipeline* shader)
{
    for (int i = 0; i < shader->stages.size(); ++i)
    {
        auto& stage = shader->stages[i];

        if (stage.shader_resource.is_valid())
        {
            g_runtime->gpu->destroy_shader(g_runtime->device, stage.shader_resource);
        }
    }

    if (shader->render_pass_resource.is_valid())
    {
        g_runtime->gpu->destroy_render_pass(g_runtime->device, shader->render_pass_resource);
    }
    if (shader->pipeline_resource.is_valid())
    {
        g_runtime->gpu->destroy_pipeline_state(g_runtime->device, shader->pipeline_resource);
    }
}


ShaderPipelineHandle get_shader(ShaderCache* cache, const u32 name_hash)
{
    scoped_recursive_lock_t lock(cache->mutex);
    auto* handle = cache->lookup.find(name_hash);
    return handle == nullptr ? ShaderPipelineHandle{} : handle->value;
}

u32 get_shader_hash(ShaderCache* cache, const ShaderPipelineHandle handle)
{
    scoped_recursive_lock_t lock(cache->mutex);
    return get_shader_name_hash(cache->pool[handle].name.view());
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

PipelineStateHandle get_pipeline_state(const ShaderPipelineHandle shader)
{
    return PipelineStateHandle{};
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
static ShaderModule     g_shader_module{};
ShaderCacheModule       g_shader_cache{}; // extern elsewhere in module

void load_shader_modules(bee::PluginLoader* loader, const bee::PluginState state)
{
    g_runtime = loader->get_static<RuntimeShaderData>("Bee.RuntimeShaderData");

    // Shader module
    g_shader_module.init = bee::init;
    g_shader_module.load = bee::load_shader;
    g_shader_module.unload = bee::unload_shader;
    loader->set_module(BEE_SHADER_MODULE_NAME, &g_shader_module, state);

    // ShaderCache module
    g_shader_cache.create = create;
    g_shader_cache.destroy = destroy;
    g_shader_cache.load = load_cache;
    g_shader_cache.save = save_cache;
    g_shader_cache.add_shader = add_shader;
    g_shader_cache.remove_shader = remove_shader;
    g_shader_cache.get_shader = get_shader;
    g_shader_cache.get_shader_name_hash = get_shader_name_hash;
    g_shader_cache.get_shader_hash = get_shader_hash;

    loader->set_module(BEE_SHADER_CACHE_MODULE_NAME, &g_shader_cache, state);
}

} // namespace bee