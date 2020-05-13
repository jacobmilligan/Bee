/*
 *  AssetPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipeline/AssetPipeline.hpp"
#include "Bee/AssetPipeline/AssetCompiler.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"

namespace bee {


// Forward declare here to ensure only the asset pipeline can open and close the assetdb
void assetdb_open(const Path& root, AssetCompilerRegistry* compiler_registry);

void assetdb_close();

/*
 **************************
 *
 * Asset Pipeline
 *
 **************************
 */
AssetPipeline::~AssetPipeline()
{
    destroy();
}

void AssetPipeline::module_observer(const PluginEventType type, const char* plugin_name, void* interface, void* user_data)
{
    auto* pipeline = static_cast<AssetPipeline*>(user_data);
    auto* module = static_cast<AssetPipelineModule*>(interface);

    if (type == PluginEventType::add_module)
    {
        // register compilers first in case the import_assets call needs to use one from the plugin
        if (module->register_compilers != nullptr)
        {
            BEE_ASSERT_F(module->unregister_compilers != nullptr, "If an asset pipeline module registers compilers, it must also unregister them");

            // wait on all import jobs to finish before modifying compilers
            job_wait(&pipeline->import_jobs_);
            module->register_compilers(&pipeline->compilers_);
        }

        // import all the plugins assets
        if (module->import_assets != nullptr)
        {
            BEE_ASSERT_F(module->delete_assets != nullptr, "If an asset pipeline module imports assets it must also delete them to clean up");
            module->import_assets(pipeline);
        }
    }

    if (type == PluginEventType::remove_module)
    {
        if (module->unregister_compilers != nullptr)
        {
            job_wait(&pipeline->import_jobs_);
            module->unregister_compilers(&pipeline->compilers_);
        }

        if (module->delete_assets != nullptr)
        {
            module->delete_assets(pipeline);
        }
    }
}


void AssetPipeline::init(const AssetPipelineInitInfo& info)
{
    BEE_ASSERT(info.platform != AssetPlatform::unknown);

    if (BEE_FAIL_F(!is_initialized(), "Asset Pipeline is already initialized at %s", assets_root_.c_str()))
    {
        return;
    }

    if (db_.is_open())
    {
        db_.close();
    }

    assets_root_ = info.asset_root;
    platform_ = info.platform;
    cache_root_ = info.cache_directory;

    db_.open(cache_root_, info.asset_database_name);

    //add_plugin_observer(BEE_ASSET_PIPELINE_MODULE_NAME, module_observer, this);
}

void AssetPipeline::destroy()
{
    if (!is_initialized())
    {
        return;
    }

    //remove_plugin_observer(BEE_ASSET_PIPELINE_MODULE_NAME, module_observer);

    job_wait(&import_jobs_);
    compilers_.clear();

    if (db_.is_open())
    {
        db_.close();
    }

    assets_root_.clear();
    platform_ = AssetPlatform::unknown;
}

void AssetPipeline::set_platform(const AssetPlatform platform)
{
    job_wait(&import_jobs_);
    platform_ = platform;
}

void AssetPipeline::import_asset(const Path& source_path, const Path& dst_path, const StringView& name)
{
    auto* job = create_job(import_job, this, source_path, dst_path, name);
    job_schedule(&import_jobs_, job);
}

void AssetPipeline::delete_asset(const GUID& guid, const DeleteAssetKind kind)
{
    job_wait(&import_jobs_);

    auto txn = db_.write();

    AssetFile asset(temp_allocator());
    if (!txn.get_asset(guid, &asset))
    {
        log_error("Failed to delete asset");
        return;
    }

    // Delete from disk if needed
    if (kind == DeleteAssetKind::asset_and_source)
    {
        const auto src_abs_path = assets_root_.join(asset.source, temp_allocator());
        if (!src_abs_path.exists())
        {
            log_error("Failed to delete asset: invalid source path %s", src_abs_path.c_str());
            return;
        }

        if (BEE_FAIL(fs::remove(src_abs_path)))
        {
            return;
        }
    }

    // Delete from database
    txn.delete_asset(guid);
    txn.commit();

    const auto location_abs_path = assets_root_.join(asset.location, temp_allocator());

    // Delete .asset file
    if (!fs::remove(asset.location))
    {
        log_error("Failed to delete asset: invalid .asset file location %s", location_abs_path.c_str());
    }
}

void AssetPipeline::delete_asset(const StringView& name, const DeleteAssetKind kind)
{
    job_wait(&import_jobs_);

    auto txn = db_.read();

    GUID guid{};
    if (!txn.get_guid_from_name(name, &guid))
    {
        log_error("Failed to delete asset: no GUID found for asset with name \"%" BEE_PRIsv "\"", BEE_FMT_SV(name));
        return;
    }

    txn.commit();

    delete_asset(guid, kind);
}

void write_asset_to_disk(const Path& dst, AssetFile* asset)
{
    io::FileStream filestream(dst, "wb");
    StreamSerializer serializer(&filestream);
    serialize(SerializerMode::writing, &serializer, asset);
}

void read_asset_from_disk(const Path& dst, AssetFile* asset)
{
    io::FileStream filestream(dst, "rb");
    StreamSerializer serializer(&filestream);
    serialize(SerializerMode::reading, &serializer, asset);
}

void AssetPipeline::import_job(AssetPipeline* pipeline, const Path& src, const Path& dst, const StringView& name)
{
    if (!src.exists())
    {
        log_error("Failed to import asset: %s is not a valid source path", src.c_str());
        return;
    }

    const auto dst_dir = pipeline->assets_root_.join(dst, temp_allocator());
    const auto dst_abs_path = dst_dir.join(src.filename()).set_extension("asset");
    const auto dst_rel_path = dst_abs_path.relative_to(pipeline->assets_root_, temp_allocator());
    const auto src_rel_path = src.relative_to(dst_abs_path, temp_allocator());

    if (!dst_dir.exists())
    {
        if (!fs::mkdir(dst_dir, true))
        {
            log_error("Failed to import asset %s: invalid dst path %s specified", dst.c_str(), src.c_str());
            return;
        }
    }

    AssetFile asset(temp_allocator());

    if (dst_abs_path.exists())
    {
        read_asset_from_disk(dst_abs_path, &asset);
    }
    else
    {
        asset.guid = generate_guid();
        asset.source = src_rel_path;
        asset.location = dst_rel_path;
    }

    // Assign the new name
    asset.name = name;

    AssetCompilerContext ctx(pipeline->platform_, src.view(), pipeline->cache_root_.view(), {}, temp_allocator());
    const auto status = pipeline->compilers_.compile(&ctx);

    if (status != AssetCompilerStatus::success)
    {
        log_error("Failed to import asset %s: %s", src.c_str(), enum_to_type(status).name);
        return;
    }

    auto txn = pipeline->db_.write();

    // Delete any mismatching artifacts from the existing asset if it has any
    for (auto& hash : asset.artifacts)
    {
        const auto index = container_index_of(ctx.artifacts(), [&](const AssetArtifact& artifact)
        {
            return artifact.hash == hash;
        });

        if (index < 0)
        {
            txn.delete_artifact(hash);
        }
    }

    asset.artifacts.clear();

    // Put the new artifacts
    for (const auto& artifact : ctx.artifacts())
    {
        txn.put_artifact(artifact);
        asset.artifacts.push_back(artifact.hash);
    }

    // Write the asset file to the database and then persist json to disk
    txn.put_asset(asset);
    write_asset_to_disk(dst_abs_path, &asset);

    txn.commit();
}


} // namespace bee