/*
 *  AssetPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

#include "Bee/AssetDatabase/AssetDatabase.hpp"
#include "Bee/AssetPipeline/AssetPipeline.hpp"
#include "Bee/AssetPipeline/AssetPipeline.inl"


namespace bee {


AssetDatabaseModule*    g_assetdb = nullptr;
AssetCacheModule*       g_asset_cache = nullptr;

/*
 **********************************
 *
 * Temp alloc - stack-style scoped
 * temporary allocation
 *
 **********************************
 */
TempAlloc::TempAlloc(AssetPipeline* pipeline)
{
    allocator_ = &pipeline->thread_data[job_worker_id()].temp_allocator;
    offset_ = allocator_->offset();
}

TempAlloc::~TempAlloc()
{
    allocator_->reset_offset(offset_);
}

/*
 **********************************
 *
 * Asset pipeline implementation
 *
 **********************************
 */
static constexpr const char* g_artifacts_dirname = "Artifacts";

static void save_config(AssetPipeline* pipeline)
{
    TempAlloc temp_alloc(pipeline);
    JSONSerializer serializer(temp_alloc);
    serialize(SerializerMode::writing, &serializer, &pipeline->config);
    fs::write(pipeline->config_path, serializer.c_str());
}

static void load_config(AssetPipeline* pipeline)
{
    TempAlloc temp_alloc(pipeline);
    auto contents = fs::read(pipeline->config_path, temp_alloc);
    JSONSerializer serializer(contents.data(), rapidjson::kParseInsituFlag, temp_alloc);
    serialize(SerializerMode::reading, &serializer, &pipeline->config);
}

static bool init_pipeline(AssetPipeline* pipeline)
{
    pipeline->thread_data.resize(job_system_worker_count());
    pipeline->full_cache_path = pipeline->config_path.parent_path();
    pipeline->full_cache_path.append(pipeline->config.cache_root.view());

    if (!pipeline->full_cache_path.exists())
    {
        fs::mkdir(pipeline->full_cache_path, true);
    }

    pipeline->db_path = pipeline->full_cache_path.join("AssetDB");

    // Open the asset database instance
    pipeline->db = g_assetdb->open(pipeline->db_path);
    if (pipeline->db == nullptr)
    {
        return false;
    }

    // start the source asset watcher after adding all the new source roots
    for (auto& source_root : pipeline->config.source_roots)
    {
        pipeline->source_watcher.add_directory(pipeline->config_path.parent_path().append(source_root.view()));
    }
    pipeline->source_watcher.start(pipeline->config.name.c_str());

    return pipeline;
}

AssetPipeline* create_pipeline(const AssetPipelineInfo& info)
{
    auto* pipeline = BEE_NEW(system_allocator(), AssetPipeline);
    pipeline->config_path = info.config_path;
    pipeline->config.cache_root = info.cache_root;
    pipeline->config.name = info.name;

    // Create directories if missing
    if (BEE_FAIL(!pipeline->config_path.exists()))
    {
        BEE_DELETE(system_allocator(), pipeline);
        return nullptr;
    }

    for (int i = 0; i < info.source_root_count; ++i)
    {
        pipeline->config.source_roots.back().append(info.source_roots[i]);
    }

    if (BEE_FAIL(init_pipeline(pipeline)))
    {
        BEE_DELETE(system_allocator(), pipeline);
        return nullptr;
    }

    save_config(pipeline);

    return pipeline;
}

AssetPipeline* load_pipeline(const StringView path)
{
    auto* pipeline = BEE_NEW(system_allocator(), AssetPipeline);
    pipeline->config_path = path;
    pipeline->thread_data.resize(job_system_worker_count());

    if (BEE_FAIL(fs::is_file(pipeline->config_path)))
    {
        BEE_DELETE(system_allocator(), pipeline);
        return nullptr;
    }

    load_config(pipeline);

    if (!init_pipeline(pipeline))
    {
        BEE_DELETE(system_allocator(), pipeline);
        return nullptr;
    }

    return pipeline;
}

void destroy_pipeline(AssetPipeline* pipeline)
{
    if (pipeline->runtime_cache != nullptr)
    {
        BEE_ASSERT(g_asset_cache != nullptr);
        g_asset_cache->unregister_locator(pipeline->runtime_cache, &pipeline->runtime_locator);
    }

    pipeline->source_watcher.stop();
    g_assetdb->close(pipeline->db);
    BEE_DELETE(system_allocator(), pipeline);
}

AssetDatabase* get_asset_db(AssetPipeline* pipeline)
{
    return pipeline->db;
}

static FileTypeInfo& add_file_type(AssetPipeline* pipeline, const char* file_type, const u32 hash)
{
    pipeline->file_type_hashes.push_back(hash);
    pipeline->file_types.emplace_back();
    auto& info = pipeline->file_types.back();
    info.extension = file_type;
    return info;
}

void register_importer(AssetPipeline* pipeline, AssetImporter* importer, void* user_data)
{
    const u32 hash = get_hash(importer->name());
    const int existing_index = find_index(pipeline->importer_hashes, hash);

    if (BEE_FAIL(existing_index < 0))
    {
        return;
    }

    pipeline->importer_hashes.push_back(hash);
    pipeline->importers.emplace_back();
    pipeline->importers.back().importer = importer;
    pipeline->importers.back().user_data = user_data;

    int file_type_count = importer->supported_file_types(nullptr);
    auto* file_types = BEE_ALLOCA_ARRAY(const char*, file_type_count);
    importer->supported_file_types(file_types);

    // Register all the supported file types and importer mappings
    for (int i = 0; i < file_type_count; ++i)
    {
        const u32 ft_hash = get_hash(file_types[i]);
        const int index = find_index(pipeline->file_type_hashes, ft_hash);
        if (index < 0)
        {
            auto& info = add_file_type(pipeline, file_types[i], ft_hash);
            info.importer_hashes.push_back(hash);
        }
        else
        {
            pipeline->file_types[index].importer_hashes.push_back(hash);
        }

        pipeline->file_type_hashes.push_back(ft_hash);
        pipeline->importers.back().file_type_hashes.push_back(ft_hash);
    }
}

void unregister_importer(AssetPipeline* pipeline, AssetImporter* importer)
{
    const u32 hash = get_hash(importer->name());
    const int index = find_index(pipeline->importer_hashes, hash);

    if (BEE_FAIL(index >= 0))
    {
        return;
    }

    auto& info = pipeline->importers[index];
    for (auto& ft_hash : info.file_type_hashes)
    {
        const int ft_index = find_index(pipeline->file_type_hashes, ft_hash);
        BEE_ASSERT(ft_index >= 0);

        auto& file_type = pipeline->file_types[ft_index];
        const int mapped_index = find_index(file_type.importer_hashes, hash);
        BEE_ASSERT(mapped_index >= 0);

        file_type.importer_hashes.erase(mapped_index);

        // Erase the file type if this is the last importer registered for it
        if (file_type.importer_hashes.empty())
        {
            pipeline->file_types.erase(ft_index);
            pipeline->file_type_hashes.erase(ft_index);
        }
    }

    // unregister the importer
    pipeline->importer_hashes.erase(index);
    pipeline->importers.erase(index);
}

i32 get_default_importer_for_file_type(AssetPipeline* pipeline, const StringView& ext)
{
    const u32 hash = get_hash(ext);
    for (int i = 0; i < pipeline->importers.size(); ++i)
    {
        if (find_index(pipeline->importers[i].file_type_hashes, hash) >= 0)
        {
            return i;
        }
    }
    return -1;
}

ImportErrorStatus import_asset(AssetPipeline* pipeline, const StringView path, const AssetPlatform platform)
{
    auto txn = g_assetdb->write(pipeline->db);
    AssetMetadata* meta = nullptr;

    TempAlloc tmp(pipeline);
    Path metadata_path(path, tmp);
    metadata_path.append_extension(".meta");

    // Read the GUID from disk and modify using the read-in metadata
    if (metadata_path.exists())
    {
        auto meta_file = fs::read(metadata_path, tmp);
        JSONSerializer serializer(meta_file.data(), rapidjson::kParseInsituFlag, tmp);
        auto res = g_assetdb->modify_serialized_asset(&txn, &serializer);
        if (res)
        {
            meta = res.unwrap();
        }
    }

    const auto ext = path_get_extension(path);
    int importer_index = get_default_importer_for_file_type(pipeline, ext);

    if (importer_index < 0)
    {
        return ImportErrorStatus::unsupported_file_type;
    }

    if (meta == nullptr)
    {
        // If the metadata doesn't exist then this is a new asset so create one in the database and grab the default importer
        auto& importer_info = pipeline->importers[importer_index];
        meta = g_assetdb->create_asset(&txn, importer_info.importer->properties_type()).unwrap();
        meta->importer = get_hash(importer_info.importer->name());
        meta->source.append(path).relative_to(metadata_path.view());
        meta->source.make_generic();

        // write the .meta out to file
        JSONSerializer serializer(tmp);
        serialize(SerializerMode::writing, &serializer, meta, tmp);
        if (!fs::write(metadata_path, serializer.c_str()))
        {
            return ImportErrorStatus::failed_to_write_metadata;
        }
    }

    int registered_importer = find_index_if(pipeline->importers, [&](const AssetImporterInfo& info)
    {
        return meta->importer == get_hash(info.importer->name());
    });

    // Use the importer specified in the existing metadata if one is valid otherwise use the default
    if (registered_importer >= 0)
    {
        importer_index = registered_importer;
    }

    AssetImportContext ctx{};
    ctx.temp_allocator = tmp;
    ctx.platform = platform;
    ctx.metadata = meta;
    ctx.db = g_assetdb;
    ctx.txn = &txn;
    ctx.artifact_buffer = &pipeline->thread_data[job_worker_id()].artifact_buffer;
    ctx.artifact_buffer->clear();
    ctx.path = path;

    const auto status = pipeline->importers[importer_index].importer->import(&ctx, pipeline->importers[importer_index].user_data);
    if (status != ImportErrorStatus::success)
    {
        return status;
    }

    txn.commit();

    if (g_asset_cache != nullptr)
    {
        auto load_res = g_asset_cache->load_asset(pipeline->runtime_cache, meta->guid);
        if (!load_res)
        {
            log_error("Failed to load asset at %s: %s", path.c_str(), load_res.unwrap_error().to_string());
        }
    }

    return ImportErrorStatus::success;
}

void refresh(AssetPipeline* pipeline)
{
    pipeline->source_watcher.pop_events(&pipeline->fs_events);

    for (auto& event : pipeline->fs_events)
    {
        switch (event.action)
        {
            case fs::FileAction::added:
            case fs::FileAction::modified:
            {
                import_asset(pipeline, event.file.view(), AssetPlatform::unknown);
                break;
            }
            case fs::FileAction::removed:
            {
                // TODO(Jacob): get guid from path and delete
                break;
            }
            default: break;
        }
    }
}

static bool asset_db_locate(void* user_data, const GUID guid, AssetLocation* location)
{
    auto* pipeline = static_cast<AssetPipeline*>(user_data);
    auto txn = g_assetdb->read(pipeline->db);
    if (!g_assetdb->asset_exists(&txn, guid))
    {
        return false;
    }

    location->streams.size = g_assetdb->get_artifacts(&txn, guid, nullptr).unwrap();
    auto* artifacts = BEE_ALLOCA_ARRAY(AssetArtifact, location->streams.size);
    g_assetdb->get_artifacts(&txn, guid, artifacts);

    location->type = get_type(artifacts[0].type_hash);

    for (int i = 0; i < location->streams.size; ++i)
    {
        location->streams[i].kind = AssetStreamKind::file;
        location->streams[i].hash = artifacts[i].content_hash;
        g_assetdb->get_artifact_path(&txn, artifacts[i].content_hash, &location->streams[i].path);
    }

    return true;
}

void set_runtime_cache(AssetPipeline* pipeline, AssetCache* cache)
{
    if (g_asset_cache == nullptr)
    {
        g_asset_cache = static_cast<bee::AssetCacheModule*>(get_module(BEE_ASSET_CACHE_MODULE_NAME));
        if (BEE_FAIL_F(g_asset_cache != nullptr, "Bee.AssetCache plugin is not loaded"))
        {
            return;
        }
    }

    if (pipeline->runtime_cache != nullptr)
    {
        g_asset_cache->unregister_locator(pipeline->runtime_cache, &pipeline->runtime_locator);
    }

    if (cache == nullptr)
    {
        pipeline->runtime_cache = nullptr;
        return;
    }

    pipeline->runtime_cache = cache;
    pipeline->runtime_locator.locate = asset_db_locate;
    pipeline->runtime_locator.user_data = pipeline;
    g_asset_cache->register_locator(cache, &pipeline->runtime_locator);
}


} // namespace bee

static bee::AssetPipelineModule g_module{};

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    if (!loader->require_plugin("Bee.AssetDatabase", { 0, 0, 0 }))
    {
        return;
    }

    bee::g_assetdb = static_cast<bee::AssetDatabaseModule*>(loader->get_module(BEE_ASSET_DATABASE_MODULE_NAME));

    g_module.create_pipeline = bee::create_pipeline;
    g_module.load_pipeline = bee::load_pipeline;
    g_module.destroy_pipeline = bee::destroy_pipeline;
    g_module.register_importer = bee::register_importer;
    g_module.unregister_importer = bee::unregister_importer;
    g_module.import_asset = bee::import_asset;
    g_module.refresh = bee::refresh;
    g_module.set_runtime_cache = bee::set_runtime_cache;

    loader->set_module(BEE_ASSET_PIPELINE_MODULE_NAME, &g_module, state);
}

BEE_PLUGIN_VERSION(0, 0, 0);