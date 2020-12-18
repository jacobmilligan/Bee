/*
 *  AssetPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipeline/AssetPipeline.inl"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

#include "Bee/AssetDatabase/AssetDatabase.hpp"


namespace bee {


AssetDatabaseModule* g_assetdb = nullptr;

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

AssetPipeline* create_pipeline(const AssetPipelineInfo& info)
{
    auto* pipeline = BEE_NEW(system_allocator(), AssetPipeline);
    pipeline->config_path = info.config_path;
    pipeline->config.cache_root = info.cache_root;
    pipeline->config.name = info.name;
    pipeline->db_path = pipeline->config.cache_root.join(info.name);
    pipeline->thread_data.resize(job_system_worker_count());

    TempAlloc temp_alloc(pipeline);

    // Create directories if missing
    if (BEE_FAIL(!pipeline->config_path.exists()))
    {
        BEE_DELETE(system_allocator(), pipeline);
        return nullptr;
    }

    if (!pipeline->config.cache_root.exists())
    {
        fs::mkdir(pipeline->config.cache_root, true);
    }

    if (!g_assetdb->open(pipeline->db_path))
    {
        BEE_DELETE(system_allocator(), pipeline);
        return nullptr;
    }

    for (int i = 0; i < info.source_root_count; ++i)
    {
        pipeline->config.source_roots.push_back(info.source_roots[i]);
    }

    save_config(pipeline);
    pipeline->source_watcher.start(pipeline->config.name.c_str());

    return pipeline;
}

AssetPipeline* load_pipeline(const StringView path)
{
    auto* pipeline = BEE_NEW(system_allocator(), AssetPipeline);
    pipeline->config_path = path;

    if (BEE_FAIL(fs::is_file(pipeline->config_path)))
    {
        BEE_DELETE(system_allocator(), pipeline);
        return nullptr;
    }

    load_config(pipeline);

    pipeline->db_path = pipeline->config.cache_root.join(pipeline->config.name.view());
    pipeline->artifact_root = pipeline->config.cache_root.join(g_artifacts_dirname);
    pipeline->thread_data.resize(job_system_worker_count());

    // start the source asset watcher after adding all the existing source roots
    for (auto& source_root : pipeline->config.source_roots)
    {
        pipeline->source_watcher.add_directory(source_root);
    }

    pipeline->source_watcher.start(pipeline->config.name.c_str());

    return pipeline;
}

void destroy_pipeline(AssetPipeline* pipeline)
{
    pipeline->source_watcher.stop();
    g_assetdb->close(pipeline->db);
    BEE_DELETE(system_allocator(), pipeline);
}

static FileTypeInfo& add_file_type(AssetPipeline* pipeline, const char* file_type, const u32 hash)
{
    pipeline->file_type_hashes.push_back(hash);
    pipeline->file_types.emplace_back();
    auto& info = pipeline->file_types.back();
    info.extension = file_type;
    return info;
}

void register_importer(AssetPipeline* pipeline, AssetImporter* importer)
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
        if (find_index(pipeline->importers[i].file_type_hashes, ext_hash) >= 0)
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
    auto full_path = Path(pipeline->config_path.parent_view(), tmp).append(path);
    Path metadata_path(full_path, tmp);
    metadata_path.append_extension(".meta");

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

    const auto ext = path_get_extension(full_path);
    int importer_index = get_default_importer_for_file_type(ext);

    if (importer_index < 0)
    {
        return ImportErrorStatus::unsupported_file_type;
    }

    if (ctx.metadata != nullptr)
    {
        int serialized_importer = find_index_if(pipeline->importers, [&](const AssetImporterInfo& info)
        {
            return get_hash(info.importer->name());
        });
        if (serialized_importer >= 0)
        {
            importer_index = serialized_importer;
        }
    }
    else
    {
        auto& importer_info = pipeline->importers[importer_index];
        meta = g_assetdb->create_asset(&txn, importer_info.importer->properties_type()).unwrap();
        meta->importer = get_hash(importer_info.importer.name());
        meta->source.append(path);

        JSONSerializer serializer(tmp);
        serialize(SerializerMode::writing, &serializer, meta, tmp);
        if (!fs::write(metadata_path, serializer.c_str()))
        {
            return ImportErrorStatus::failed_to_write_metadata;
        }
    }

    AssetImportContext ctx{};
    ctx.temp_allocator = tmp;
    ctx.platform = platform;
    ctx.metadata = meta;
    ctx.db = pipeline->db;
    ctx.txn = &txn;

    const auto status = pipeline->importers[importer_index].importer->import(&ctx);
    if (status != ImportErrorStatus::success)
    {
        return status;
    }

    txn.commit();
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
                import_asset(pipeline, event.file.view());
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


} // namespace bee

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    if (!loader->require_plugin("Bee.AssetDatabase", { 0, 0, 0 }))
    {
        return;
    }

    bee::g_assetdb = static_cast<bee::AssetDatabaseModule*>(loader->get_module(BEE_ASSET_DATABASE_MODULE_NAME));
}

BEE_PLUGIN_VERSION(0, 0, 0);