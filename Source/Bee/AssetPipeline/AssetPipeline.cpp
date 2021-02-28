/*
 *  AssetPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipeline/AssetPipeline.inl"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"


namespace bee {


// Extern asset database
extern void set_asset_database_module(bee::PluginLoader* loader, const bee::PluginState state);

// Extern other pipeline stages
extern Result<void, AssetPipelineError> init_import_pipeline(AssetPipeline* pipeline, const AssetPipelineImportInfo& info);
extern void destroy_import_pipeline(AssetPipeline* pipeline);
extern Result<void, AssetPipelineError> refresh_import_pipeline(AssetPipeline* pipeline);
extern void set_import_pipeline(AssetPipelineModule* module, PluginLoader* loader, const PluginState state);

extern Result<void, AssetPipelineError> init_load_pipeline(AssetPipeline* pipeline);
extern void destroy_load_pipeline(AssetPipeline* pipeline);
extern Result<void, AssetPipelineError> refresh_load_pipeline(AssetPipeline* pipeline);
extern void set_load_pipeline(AssetPipelineModule* module, PluginLoader* loader, const PluginState state);

/*
 **********************************
 *
 * Asset Pipeline implementation
 *
 **********************************
 */
bool AssetPipeline::can_import() const
{
    return (flags & AssetPipelineFlags::import) != AssetPipelineFlags::none;
}

bool AssetPipeline::can_load() const
{
    return (flags & AssetPipelineFlags::load) != AssetPipelineFlags::none;
}

AssetPipeline::ThreadData& AssetPipeline::get_thread()
{
    return thread_data[job_worker_id()];
}

Result<AssetPipeline*, AssetPipelineError> create_pipeline(const AssetPipelineInfo& info)
{
    auto* pipeline = BEE_NEW(system_allocator(), AssetPipeline);
    pipeline->flags = info.flags;
    pipeline->thread_data.resize(job_system_worker_count());

    if (pipeline->can_import())
    {
        BEE_ASSERT(info.import != nullptr);

        auto res = init_import_pipeline(pipeline, *info.import);
        if (!res)
        {
            BEE_DELETE(system_allocator(), pipeline);
            return res.unwrap_error();
        }
    }

    if (pipeline->can_load())
    {
        auto res = init_load_pipeline(pipeline);
        if (!res)
        {
            BEE_DELETE(system_allocator(), pipeline);
            return res.unwrap_error();
        }
    }

    return pipeline;
}

void destroy_pipeline(AssetPipeline* pipeline)
{
    if (pipeline->can_import())
    {
        destroy_import_pipeline(pipeline);
        destruct(&pipeline->import);
    }

    if (pipeline->can_load())
    {
        destroy_load_pipeline(pipeline);
        destruct(&pipeline->load);
    }

    BEE_DELETE(system_allocator(), pipeline);
}

AssetPipelineFlags get_flags(const AssetPipeline* pipeline)
{
    return pipeline->flags;
}

Result<void, AssetPipelineError> refresh(AssetPipeline* pipeline)
{
    if (pipeline->can_import())
    {
        auto res = refresh_import_pipeline(pipeline);
        if (!res)
        {
            return res.unwrap_error();
        }
    }

    if (pipeline->can_load())
    {
        auto res = refresh_load_pipeline(pipeline);
        if (!res)
        {
            return res.unwrap_error();
        }
    }

    return {};
}


} // namespace bee

static bee::AssetPipelineModule g_module{};


BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    g_module.create_pipeline = bee::create_pipeline;
    g_module.destroy_pipeline = bee::destroy_pipeline;
    g_module.get_flags = bee::get_flags;
    g_module.refresh = bee::refresh;

    bee::set_asset_database_module(loader, state);
    bee::set_import_pipeline(&g_module, loader, state);
    bee::set_load_pipeline(&g_module, loader, state);

    loader->set_module(BEE_ASSET_PIPELINE_MODULE_NAME, &g_module, state);
}