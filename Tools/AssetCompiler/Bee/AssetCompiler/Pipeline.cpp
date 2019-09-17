/*
 *  Pipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Filesystem.hpp"
#include "Bee/AssetCompiler/Pipeline.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

namespace bee {


bool validate_pipeline_plugin(const AssetPipelinePlugin& desc)
{
    if (desc.name == nullptr)
    {
        log_error("Bee Asset Compiler: asset compiler descriptor must have a valid name");
        return false;
    }

    if (desc.supported_file_type_count <= 0 || desc.supported_file_types == nullptr)
    {
        log_error("Bee Asset Compiler: asset compiler descriptors must support at least one valid file type");
        return false;
    }

    if (desc.create_compiler == nullptr || desc.compile == nullptr)
    {
        log_error("Bee Asset Compiler: asset compiler descriptors must have valid pointers to `create_instance` and `compile` functions");
        return false;
    }

    return true;
}

bool AssetPipeline::load_plugin(const char* directory, const char* filename)
{
#if BEE_OS_WINDOWS == 1
    static constexpr const char* plugin_file_type = ".dll";
#else
    static constexpr const char* plugin_file_type = ".dylib";
#endif // BEE_OS_WINDOWS == 1

    log_info("Bee Asset Compiler: loading plugin: %s...", filename);

    // We need to wait until all jobs are complete before loading a new plugin
    if (root_job != nullptr)
    {
        log_info("Bee Asset Compiler: waiting for executing jobs to finish...");
        job_wait(root_job);
        root_job = nullptr;
    }

    Path search_path(directory, temp_allocator());
    auto lib_file = Path(filename, temp_allocator()).set_extension(plugin_file_type);
    Path full_path(temp_allocator());

    for (const auto& iter : fs::read_dir(search_path))
    {
        if (fs::is_file(iter) && iter.filename() == lib_file)
        {
            full_path = iter;
            break;
        }
    }

    if (full_path.empty())
    {
        log_error("Bee Asset Compiler: failed to find pipeline plugin at path %s with filename: %s", directory, filename);
        return false;
    }

    auto plugin = load_library(full_path.c_str());
    if (plugin.handle == nullptr)
    {
        log_error("Bee Asset Compiler: failed to load pipeline plugin: %s", filename);
        return false;
    }

    // Load up the two entry symbols (load/unload plugin)
    auto symbol_string = String("bee_asset_compiler_load_plugin_", temp_allocator()).append(filename);
    auto load_plugin_symbol = (load_plugin_function_t*)get_library_symbol(plugin, symbol_string.c_str());
    if (load_plugin_symbol == nullptr)
    {
        unload_library(plugin); // cleanup
        log_error("Bee Asset Compiler: failed to load pipeline plugin %s: missing required symbol: %s", filename, symbol_string.c_str());
        return false;
    }

    symbol_string.clear();
    symbol_string.append("bee_asset_compiler_unload_plugin_").append(filename);
    auto unload_plugin_symbol = (unload_plugin_function_t*)get_library_symbol(plugin, symbol_string.c_str());
    if (unload_plugin_symbol == nullptr)
    {
        unload_library(plugin); // cleanup
        log_error("Bee Asset Compiler: failed to load pipeline plugin %s: missing required symbol: %s", filename, symbol_string.c_str());
        return false;
    }

    const auto desc = load_plugin_symbol();
    if (!validate_pipeline_plugin(desc))
    {
        // Invalid desc so unload the plugin and its dylib
        unload_plugin_symbol();
        unload_library(plugin);
    }

    const auto name_hash = get_hash(desc.name);
    if (plugins.find(name_hash) != nullptr)
    {
        log_error("Bee Asset Compiler: a plugin with that name is already loaded: %s", desc.name);
        return false;
    }

    // Ensure that this plugin supports types uniquely - i.e. no other plugin supports the same types
    scoped_spinlock_t filetype_lock(file_type_mutex);
    for (int ft = 0; ft < desc.supported_file_type_count; ++ft)
    {
        auto existing_filetype_support = file_type_map.find(get_hash(desc.supported_file_types[ft]));
        if (existing_filetype_support != nullptr)
        {
            scoped_spinlock_t lock(plugin_mutex);
            const char* conflicting_plugin_name = plugins.find(existing_filetype_support->value)->value.name.c_str();
            log_error(
                "Bee Asset Compiler: cannot register plugin \"%s\" - file type \"%s\" is already supported by another plugin: \"%s\"",
                desc.name,
                desc.supported_file_types[ft],
                conflicting_plugin_name
            );

            // Failure - cleanup library
            unload_plugin_symbol();
            unload_library(plugin);
            return false;
        }
    }

    // Register the plugin
    scoped_spinlock_t lock(plugin_mutex);
    auto registered = plugins.insert(name_hash, RegisteredPlugin());
    new (&registered->value) RegisteredPlugin(desc, plugin, load_plugin_symbol, unload_plugin_symbol);
    registered->value.compilers.resize(get_job_worker_count()); // allow one local compiler per worker

    // Register the file type mappings - file types are already locked here
    for (const auto& filetype_hash : registered->value.file_type_mappings)
    {
        file_type_map.insert(filetype_hash, name_hash);
    }

    return true;
}

bool AssetPipeline::unload_plugin(const char* name)
{
    log_info("Bee Asset Compiler: unloading plugin: %s...", name);

    if (root_job != nullptr)
    {
        log_info("Bee Asset Compiler: waiting for executing jobs to finish...");
        job_wait(root_job);
        root_job = nullptr;
    }

    // Ensure the plugin exists
    scoped_spinlock_t lock(plugin_mutex);
    const auto name_hash = get_hash(name);
    auto plugin = plugins.find(name_hash);
    if (plugin == nullptr)
    {
        log_error("Bee Asset Compiler: no such plugin loaded: %s", name);
        return false;
    }

    // First unregister all the file type mappings
    scoped_spinlock_t filetype_lock(file_type_mutex);
    for (const auto& filetype_hash : plugin->value.file_type_mappings)
    {
        file_type_map.erase(filetype_hash);
    }

    // Call plugins unload function and unload the dylib
    plugin->value.unload_plugin_symbol();
    unload_library(plugin->value.library);
    plugins.erase(name_hash); // destroy all held memory

    return true;
}


struct AssetCompileJob final : Job
{
    AssetPlatform            platform { AssetPlatform::unknown };
    char                     src[1024];
    AssetCompileWaitHandle*  wait_handle { nullptr };
    AssetCompilerHandle      compiler;
    asset_compile_function_t compile { nullptr };


    AssetCompileJob(
        const AssetPlatform dst_platform,
        const char* src_path,
        AssetCompileWaitHandle* wait_handle,
        const AssetCompilerHandle& compiler_handle,
        asset_compile_function_t compile_function
    ) : platform(dst_platform),
        compiler(compiler_handle),
        compile(compile_function)
    {
        str::copy(src, static_array_length(src), src_path);
    }

    void execute() override
    {
        io::MemoryStream stream(&wait_handle->data);

        AssetPipelineContext ctx{};
        ctx.location = src;
        ctx.platform = platform;
        ctx.temp_allocator = job_temp_allocator();
        ctx.stream = &stream;

        wait_handle->result = compile(compiler, &ctx);
        wait_handle->is_complete_flag.store(true, std::memory_order_seq_cst);
    }
};


bool AssetPipeline::compile(const AssetPlatform platform, const char* src, AssetCompileWaitHandle* wait_handle)
{
    const auto filetype = path_get_extension(src);
    const auto filetype_hash = get_hash(filetype);
    u32 plugin_name_hash = 0;

    scoped_spinlock_t filetype_lock(file_type_mutex);
    auto found_filetype = file_type_map.find(filetype_hash);
    if (found_filetype == nullptr)
    {
        log_error("Bee Asset Compiler: file type not supported by any plugins: %" BEE_PRIsv, BEE_FMT_SV(filetype));
        return false;
    }

    plugin_name_hash = found_filetype->value;

    scoped_spinlock_t plugin_lock(plugin_mutex);
    auto found_plugin = plugins.find(plugin_name_hash);
    if (found_plugin == nullptr)
    {
        log_error("Bee Asset Compiler: invalid plugin for filetype \"%" BEE_PRIsv "\". Removing filetype mapping...", BEE_FMT_SV(filetype));
        file_type_map.erase(filetype_hash);
        return false;
    }

    auto& local_compiler = found_plugin->value.compilers[get_local_job_worker_id()];
    if (!local_compiler.is_valid())
    {
        local_compiler = found_plugin->value.create_compiler();
    }

    auto job = allocate_job<AssetCompileJob>(platform, src, wait_handle, local_compiler, found_plugin->value.compile);
    root_job->add_dependency(job);
    schedule_job(job);
    return job;
}


} // namespace bee