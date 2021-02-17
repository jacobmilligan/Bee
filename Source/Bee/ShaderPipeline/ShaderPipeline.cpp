/*
 *  ShaderPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/ShaderPipeline/Compiler.hpp"

#include "Bee/AssetPipelineV2/AssetPipeline.hpp"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Memory/LinearAllocator.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"


namespace bee {


struct CachedSampler
{
    atomic_i32    refcount { 0 };
    SamplerHandle handle;

    CachedSampler() = default;

    CachedSampler(const SamplerHandle new_handle)
        : handle(new_handle)
    {}

    CachedSampler(const CachedSampler& other)
        : refcount(other.refcount.load(std::memory_order_relaxed)),
          handle(other.handle)
    {}

    CachedSampler& operator=(const CachedSampler& other)
    {
        refcount.store(other.refcount.load(std::memory_order_relaxed));
        handle = other.handle;
        return *this;
    }
};

struct ShaderPipeline
{
    struct ImporterThreadData
    {
        LinearAllocator         code_allocator { kilobytes(64), system_allocator() };
        DynamicArray<Shader>    shaders;
        String                  shader_name;
    };

    struct LoaderThreadData
    {
        DynamicArray<SamplerCreateInfo> samplers_to_delete;
        StaticArray<ResourceBindingUpdate, BEE_GPU_MAX_RESOURCE_BINDINGS>   sampler_updates[BEE_GPU_MAX_RESOURCE_LAYOUTS];
        StaticArray<TextureBindingUpdate, BEE_GPU_MAX_RESOURCE_BINDINGS>    texture_updates[BEE_GPU_MAX_RESOURCE_LAYOUTS];

        void add_sampler_update(const ShaderSampler& sampler)
        {
            auto& tex_update_layout = texture_updates[sampler.layout];
            auto& tex_update = tex_update_layout[tex_update_layout.size++];
            new (&tex_update) TextureBindingUpdate(sampler.handle);

            auto& sampler_update_layout = sampler_updates[sampler.layout];
            auto& sampler_update = sampler_update_layout[sampler_update_layout.size++];
            new (&sampler_update) ResourceBindingUpdate(sampler.binding, 0, 1, &tex_update);
        }
    };

    AssetPipeline*                                      asset_pipeline { nullptr };
    GpuBackend*                                         gpu { nullptr };
    DeviceHandle                                        device;

    AssetImporter                                       importer;
    FixedArray<ImporterThreadData>                      importer_threads;

    AssetLoader                                         loader;
    FixedArray<LoaderThreadData>                        loader_threads;
    DynamicHashMap<SamplerCreateInfo, CachedSampler>    shared_sampler_cache;
    DynamicHashMap<SamplerCreateInfo, CachedSampler>    locked_sampler_cache;
    RecursiveMutex                                      sampler_cache_mutex;
};

static ShaderPipeline* g_shader_pipeline = nullptr;

static AssetPipelineModule* g_asset_pipeline = nullptr;
extern ShaderCompilerModule g_shader_compiler;


/*
 **********************************
 *
 * SamplerCache
 *
 **********************************
 */
SamplerHandle acquire_sampler(const SamplerCreateInfo& info)
{
    auto* cached = g_shader_pipeline->shared_sampler_cache.find(info);
    if (cached != nullptr)
    {
        cached->value.refcount.fetch_add(1, std::memory_order_relaxed);
        return cached->value.handle;
    }

    scoped_recursive_lock_t lock(g_shader_pipeline->sampler_cache_mutex);
    cached = g_shader_pipeline->locked_sampler_cache.find(info);

    if (cached == nullptr)
    {
        const auto handle = g_shader_pipeline->gpu->create_sampler(g_shader_pipeline->device, info);
        cached = g_shader_pipeline->locked_sampler_cache.insert(info, handle);
    }

    cached->value.refcount.fetch_add(1, std::memory_order_relaxed);
    return cached->value.handle;
}

void release_sampler(const SamplerCreateInfo& info)
{
    g_shader_pipeline->loader_threads[job_worker_id()].samplers_to_delete.push_back(info);
}

void sync_samplers()
{
    BEE_ASSERT(current_thread::is_main());

    for (const auto& sampler : g_shader_pipeline->locked_sampler_cache)
    {
        auto* shared = g_shader_pipeline->shared_sampler_cache.insert(sampler.key, sampler.value.handle);
        shared->value.refcount.store(sampler.value.refcount.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    g_shader_pipeline->locked_sampler_cache.clear();

    for (auto& thread : g_shader_pipeline->loader_threads)
    {
        for (const auto& to_delete : thread.samplers_to_delete)
        {
            auto* sampler = g_shader_pipeline->shared_sampler_cache.find(to_delete);
            BEE_ASSERT(sampler != nullptr);

            const int refcount = sampler->value.refcount.fetch_sub(1, std::memory_order_relaxed);
            if (refcount > 1)
            {
                continue;
            }

            g_shader_pipeline->gpu->destroy_sampler(g_shader_pipeline->device, sampler->value.handle);
            g_shader_pipeline->shared_sampler_cache.erase(to_delete);
        }

        thread.samplers_to_delete.clear();
    }
}


/*
 **********************************
 *
 * Shader Importer
 *
 **********************************
 */
static const char* get_importer_name()
{
    return "ShaderPipelineImporter";
}

static i32 get_importer_supported_file_types(const char** dst)
{
    if (dst != nullptr)
    {
        dst[0] = ".bsc";
    }
    return 1;
}

static Type get_importer_settings_type()
{
    return get_type<ShaderImportSettings>();
}

static Result<void, AssetPipelineError> import_shader(AssetImportContext* ctx, void* user_data)
{
    auto& thread = g_shader_pipeline->importer_threads[job_worker_id()];
    const auto content = fs::read(ctx->path, ctx->temp_allocator);
    const auto res = g_shader_compiler.compile_shader(
        ctx->path,
        content.view(),
        ShaderTarget::spirv,
        &thread.shaders,
        &thread.code_allocator
    );

    if (!res)
    {
        log_error("%s", res.unwrap_error().to_string());
        return { AssetPipelineError::failed_to_import };
    }

    const auto module_name = path_get_stem(ctx->path);
    auto* settings = ctx->settings->get<ShaderImportSettings>();

    for (auto& shader : thread.shaders)
    {
        auto sub_asset_res = ctx->create_sub_asset();
        if (!sub_asset_res)
        {
            return { AssetPipelineError::failed_to_import };
        }

        auto& sub_asset_ctx = sub_asset_res.unwrap();
        thread.shader_name.assign(module_name).append('.').append(shader.name.view());
        sub_asset_ctx.set_name(thread.shader_name.view());
        sub_asset_ctx.add_artifact(&shader);

        if (settings->output_debug_shaders)
        {
            String disassembly(ctx->temp_allocator);
            g_shader_compiler.disassemble_shader(ctx->path, shader, &disassembly);

            Path debug_path(ctx->cache_root, ctx->temp_allocator);
            debug_path.append("ShaderDisassembly")
                .append(ctx->target_platform_string)
                .append(thread.shader_name.view());

            if (!path_exists(debug_path.parent_view()))
            {
                if (!fs::mkdir(debug_path.parent_view(), true))
                {
                    log_error("Failed to create shader disassembly directory %" BEE_PRIsv, BEE_FMT_SV(debug_path.parent_view()));
                    continue;
                }
            }

            fs::write(debug_path, disassembly.view());
        }
    }

    // Return the code memory to the allocator and reset it
    thread.shaders.clear();
    thread.code_allocator.reset();

    return {};
}

/*
 **********************************
 *
 * Shader loader
 *
 **********************************
 */
static i32 get_shader_loader_types(Type* dst)
{
    if (dst != nullptr)
    {
        dst[0] = get_type<Shader>();
    }
    return 1;
}

void tick_shader_loader(void* user_data)
{
    sync_samplers();
}

bool load_shader(Shader* shader)
{
    ShaderCreateInfo info{};
    for (int stage_index = 0; stage_index < shader->stages.size; ++stage_index)
    {
        auto& stage = shader->stages[stage_index];
        if (stage.shader_resource.is_valid())
        {
            g_shader_pipeline->gpu->destroy_shader(g_shader_pipeline->device, stage.shader_resource);
        }

        info.code = stage.code.data();
        info.code_size = stage.code.size();
        info.entry = stage.entry.data();

        stage.shader_resource = g_shader_pipeline->gpu->create_shader(g_shader_pipeline->device, info);
        BEE_ASSERT(stage.shader_resource.is_valid());

        switch (static_cast<ShaderStageIndex>(stage_index))
        {
            case ShaderStageIndex::vertex:
            {
                shader->pipeline_desc.vertex_stage = stage.shader_resource;
                break;
            }
            case ShaderStageIndex::fragment:
            {
                shader->pipeline_desc.fragment_stage = stage.shader_resource;
                break;
            }
            default:
            {
                return false;
            }
        }
    }

    ResourceBindingCreateInfo binding_info{};
    shader->resource_bindings.size = shader->pipeline_desc.resource_layouts.size;
    for (u32 layout_index = 0; layout_index < shader->pipeline_desc.resource_layouts.size; ++layout_index)
    {
        auto& layout = shader->pipeline_desc.resource_layouts[layout_index];
        if (shader->resource_bindings[layout_index].is_valid())
        {
            g_shader_pipeline->gpu->destroy_resource_binding(g_shader_pipeline->device, shader->resource_bindings[layout_index]);
        }

        binding_info.layout = &layout;
        binding_info.update_frequency = shader->update_frequencies[layout_index];
        shader->resource_bindings[layout_index] = g_shader_pipeline->gpu->create_resource_binding(g_shader_pipeline->device, binding_info);
    }

    auto& thread = g_shader_pipeline->loader_threads[job_worker_id()];
    for (auto& update : thread.sampler_updates)
    {
        update.size = 0;
    }

    // Load up the samplers and add to cache
    for (auto& sampler : shader->samplers)
    {
        sampler.handle = acquire_sampler(sampler.info);
        thread.add_sampler_update(sampler);
    }

    for (const auto update : enumerate(thread.sampler_updates))
    {
        if (update.value.size <= 0)
        {
            continue;
        }

        const auto binding = shader->resource_bindings[update.index];
        if (binding.is_valid())
        {
            g_shader_pipeline->gpu->update_resource_binding(g_shader_pipeline->device, binding, update.value.size, update.value.data);
        }
    }

    return true;
}

void unload_shader(Shader* shader)
{
    // Release the cached sampler handles
    for (auto& sampler : shader->samplers)
    {
        release_sampler(sampler.info);
    }

    for (auto& binding : shader->resource_bindings)
    {
        if (binding.is_valid())
        {
            g_shader_pipeline->gpu->destroy_resource_binding(g_shader_pipeline->device, binding);
        }
    }

    for (const auto& stage : shader->stages)
    {
        if (stage.shader_resource.is_valid())
        {
            g_shader_pipeline->gpu->destroy_shader(g_shader_pipeline->device, stage.shader_resource);
        }
    }
}

Result<void, AssetPipelineError> shader_loader_load(const GUID guid, const AssetLocation* location, void* user_data, const AssetHandle handle, void* data)
{
    auto* shader = static_cast<Shader*>(data);

    for (auto& stream_info : location->streams)
    {
        if (stream_info.kind == AssetStreamInfo::Kind::file)
        {
            io::FileStream stream(stream_info.path, "rb");
            stream.seek(stream_info.offset, io::SeekOrigin::begin);
            StreamSerializer serializer(&stream);
            serialize(SerializerMode::reading, &serializer, shader, temp_allocator());
        }
        else
        {
            io::MemoryStream stream(stream_info.buffer, stream_info.size);
            stream.seek(stream_info.offset, io::SeekOrigin::begin);
            StreamSerializer serializer(&stream);
            serialize(SerializerMode::reading, &serializer, shader, temp_allocator());
        }
    }

    if (!load_shader(shader))
    {
        return { AssetPipelineError::invalid_data };
    }

    return {};
}

Result<void, AssetPipelineError> shader_loader_unload(const Type type, void* data, void* user_data)
{
    unload_shader(static_cast<Shader*>(data));
    return {};
}

/*
 **************************************
 *
 * ShaderPipeline API implementation
 *
 **************************************
 */
void init(AssetPipeline* asset_pipeline, GpuBackend* gpu, const DeviceHandle device)
{
    g_shader_pipeline->asset_pipeline = asset_pipeline;
    g_shader_pipeline->gpu = gpu;
    g_shader_pipeline->device = device;

    if (asset_pipeline != nullptr)
    {
        auto res = g_asset_pipeline->register_importer(asset_pipeline, &g_shader_pipeline->importer, nullptr);
        if (res)
        {
            g_shader_pipeline->importer_threads.resize(job_system_worker_count());
        }

        res = g_asset_pipeline->register_loader(asset_pipeline, &g_shader_pipeline->loader, nullptr);
        if (res)
        {
            g_shader_pipeline->loader_threads.resize(job_system_worker_count());
        }
    }
    else
    {
        // We assume this is being used free of an asset pipeline so both importer and loader *may* be used
        g_shader_pipeline->importer_threads.resize(job_system_worker_count());
        g_shader_pipeline->loader_threads.resize(job_system_worker_count());
    }

    const auto compiler_success = g_shader_compiler.init();
    BEE_ASSERT(compiler_success);
}

void shutdown()
{
    g_shader_compiler.destroy();

    if (g_shader_pipeline->asset_pipeline == nullptr)
    {
        return;
    }

    const auto flags = g_asset_pipeline->get_flags(g_shader_pipeline->asset_pipeline);

    if ((flags & AssetPipelineFlags::import) != AssetPipelineFlags::none)
    {
        g_asset_pipeline->unregister_importer(g_shader_pipeline->asset_pipeline, &g_shader_pipeline->importer);
    }

    if ((flags & AssetPipelineFlags::load) != AssetPipelineFlags::none)
    {
        g_asset_pipeline->unregister_loader(g_shader_pipeline->asset_pipeline, &g_shader_pipeline->loader);
    }

    if (!g_shader_pipeline->importer_threads.empty())
    {
        g_shader_pipeline->importer_threads.resize(0);
    }

    g_shader_pipeline->asset_pipeline = nullptr;
    g_shader_pipeline->gpu = nullptr;
    g_shader_pipeline->device = DeviceHandle{};
}

void update_resources(Shader* shader, const u32 layout, const i32 count, const ResourceBindingUpdate* updates)
{
    const auto binding = shader->resource_bindings[layout];
    if (binding.is_valid())
    {
        g_shader_pipeline->gpu->update_resource_binding(g_shader_pipeline->device, binding, count, updates);
    }
}

void bind_resource_layout(Shader* shader, CommandBuffer* cmd_buf, const u32 layout)
{
    auto* cmd = g_shader_pipeline->gpu->get_command_backend();
    const auto binding = shader->resource_bindings[layout];
    if (binding.is_valid())
    {
        cmd->bind_resources(cmd_buf, layout, shader->resource_bindings[layout]);
    }
}

void bind_resources(Shader* shader, CommandBuffer* cmd_buf)
{
    auto* cmd = g_shader_pipeline->gpu->get_command_backend();
    for (int i = 0; i < shader->resource_bindings.size; ++i)
    {
        if (shader->resource_bindings[i].is_valid())
        {
            cmd->bind_resources(cmd_buf, i, shader->resource_bindings[i]);
        }
    }
}

/*
 **************************************
 *
 * Plugin-internal extern modules
 *
 **************************************
 */
extern void load_compiler_module(PluginLoader* loader, const PluginState state);


} // namespace bee


static bee::ShaderPipelineModule g_module{};

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    bee::g_shader_pipeline = loader->get_static<bee::ShaderPipeline>("BeeShaderPipeline");

    bee::g_shader_pipeline->importer.import = bee::import_shader;
    bee::g_shader_pipeline->importer.name = bee::get_importer_name;
    bee::g_shader_pipeline->importer.settings_type = bee::get_importer_settings_type;
    bee::g_shader_pipeline->importer.supported_file_types = bee::get_importer_supported_file_types;

    bee::g_shader_pipeline->loader.load = bee::shader_loader_load;
    bee::g_shader_pipeline->loader.unload = bee::shader_loader_unload;
    bee::g_shader_pipeline->loader.get_types = bee::get_shader_loader_types;
    bee::g_shader_pipeline->loader.tick = bee::tick_shader_loader;

    g_module.init = bee::init;
    g_module.shutdown = bee::shutdown;
    g_module.bind_resource_layout = bee::bind_resource_layout;
    g_module.bind_resources = bee::bind_resources;
    g_module.update_resources = bee::update_resources;
    g_module.load_shader = bee::load_shader;
    g_module.unload_shader = bee::unload_shader;

    bee::load_compiler_module(loader, state);

    loader->set_module(BEE_SHADER_PIPELINE_MODULE_NAME, &g_module, state);

    bee::g_asset_pipeline = static_cast<bee::AssetPipelineModule*>(loader->get_module(BEE_ASSET_PIPELINE_MODULE_NAME));
}