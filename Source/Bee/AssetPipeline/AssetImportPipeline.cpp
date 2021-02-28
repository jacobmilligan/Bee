/*
 *  AssetImportPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipeline/AssetPipeline.inl"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"
#include "Bee/Core/Bit.hpp"


namespace bee {


extern AssetDatabaseModule g_assetdb;

Result<void, AssetPipelineError> init_import_pipeline(AssetPipeline* pipeline, const AssetPipelineImportInfo& info)
{
    auto& import_pipeline = pipeline->import;
    import_pipeline.cache_path = info.cache_root;
    import_pipeline.db_path = import_pipeline.cache_path.join("AssetDB");

    if (!import_pipeline.cache_path.exists())
    {
        fs::mkdir(import_pipeline.cache_path.view(), true);
    }

    // Open the asset database instance
    import_pipeline.db = g_assetdb.open(import_pipeline.db_path.view());
    if (import_pipeline.db == nullptr)
    {
        return { AssetPipelineError::asset_database };
    }

    // start the source asset watcher after adding all the new source roots
    for (int i = 0; i < info.source_root_count; ++i)
    {
        import_pipeline.source_watcher.add_directory(info.source_roots[i]);
    }

//    // watch any external source folders added before the pipeline was created
//    {
//        scoped_lock_t lock(g_common->mutex);
//        g_common->pipelines.push_back(pipeline);
//        for (const auto& path : g_common->source_folders)
//        {
//            import_pipeline.source_watcher.add_directory(path);
//        }
//    }

    import_pipeline.name = info.name;
    import_pipeline.source_watcher.start(import_pipeline.name.c_str());

    return {};
}

void destroy_import_pipeline(AssetPipeline* pipeline)
{
//    {
//        scoped_lock_t lock(g_common->mutex);
//        const int common_index = find_index(g_common->pipelines, pipeline);
//        BEE_ASSERT(common_index >= 0);
//        g_common->pipelines.erase(common_index);
//    }

    pipeline->import.source_watcher.stop();
    g_assetdb.close(pipeline->import.db);
}

static FileTypeInfo& add_file_type(ImportPipeline* pipeline, const char* file_type, const u32 hash)
{
    pipeline->file_type_hashes.push_back(hash);
    pipeline->file_types.emplace_back();
    auto& info = pipeline->file_types.back();
    info.extension = file_type;
    return info;
}

Result<void, AssetPipelineError> register_importer(AssetPipeline* pipeline, AssetImporter* importer, void* user_data)
{
    if (!pipeline->can_import())
    {
        return { AssetPipelineError::import };
    }

    auto& import_pipeline = pipeline->import;

    // Check if the importer has already been registered
    const u32 hash = get_hash(importer->name());
    const int existing_index = find_index(import_pipeline.importer_hashes, hash);

    if (existing_index >= 0)
    {
        return { AssetPipelineError::importer_registered };
    }

    // Add the importer to the pipeline
    import_pipeline.importer_hashes.push_back(hash);
    import_pipeline.importers.emplace_back();
    import_pipeline.importers.back().importer = importer;
    import_pipeline.importers.back().user_data = user_data;

    int file_type_count = importer->supported_file_types(nullptr);
    auto* file_types = BEE_ALLOCA_ARRAY(const char*, file_type_count);
    importer->supported_file_types(file_types);

    // Register all the supported file types and importer mappings
    for (int i = 0; i < file_type_count; ++i)
    {
        const u32 ft_hash = get_hash(file_types[i]);
        const int index = find_index(import_pipeline.file_type_hashes, ft_hash);
        // Add a new file type if we've never seen this one before, otherwise just add this importer to the existing
        // file types list of valid importers
        if (index < 0)
        {
            auto& info = add_file_type(&import_pipeline, file_types[i], ft_hash);
            info.importer_hashes.push_back(hash);
        }
        else
        {
            import_pipeline.file_types[index].importer_hashes.push_back(hash);
        }

        import_pipeline.file_type_hashes.push_back(ft_hash);
        import_pipeline.importers.back().file_type_hashes.push_back(ft_hash);
    }

    return {};
}

Result<void, AssetPipelineError> unregister_importer(AssetPipeline* pipeline, AssetImporter* importer)
{
    if (!pipeline->can_import())
    {
        return { AssetPipelineError::import };
    }

    auto& import_pipeline = pipeline->import;
    const u32 hash = get_hash(importer->name());
    const int index = find_index(import_pipeline.importer_hashes, hash);

    if (index < 0)
    {
        return { AssetPipelineError::importer_not_registerd };
    }

    // Remove all the file type associations for this importer before unregistering it
    auto& info = import_pipeline.importers[index];
    for (auto& ft_hash : info.file_type_hashes)
    {
        const int ft_index = find_index(import_pipeline.file_type_hashes, ft_hash);
        BEE_ASSERT(ft_index >= 0);

        auto& file_type = import_pipeline.file_types[ft_index];
        const int mapped_index = find_index(file_type.importer_hashes, hash);
        BEE_ASSERT(mapped_index >= 0);

        file_type.importer_hashes.erase(mapped_index);

        // Erase the file type if this is the last importer registered for it
        if (file_type.importer_hashes.empty())
        {
            import_pipeline.file_types.erase(ft_index);
            import_pipeline.file_type_hashes.erase(ft_index);
        }
    }

    // unregister the importer
    import_pipeline.importer_hashes.erase(index);
    import_pipeline.importers.erase(index);

    return {};
}

static i32 get_default_importer_for_file_type(const ImportPipeline& pipeline, const StringView& ext)
{
    const u32 hash = get_hash(ext);
    for (int i = 0; i < pipeline.importers.size(); ++i)
    {
        if (find_index(pipeline.importers[i].file_type_hashes, hash) >= 0)
        {
            return i;
        }
    }
    return -1;
}

Result<void, AssetPipelineError> import_asset(AssetPipeline* pipeline, const PathView& path, const AssetPlatform platform)
{
    if (!pipeline->can_import())
    {
        return { AssetPipelineError::import };
    }

    auto& thread = pipeline->get_thread();
    thread.meta_path.clear();
    thread.source_path.clear();
    if (path.extension() == ".meta")
    {
        thread.meta_path.append(path);
        // the path with extensions except for the .meta part
        thread.source_path.append(str::substring(
            path.string_view(),
            0,
            str::first_index_of(path.string_view(), ".meta")
        ));
    }
    else
    {
        thread.source_path.append(path);
        thread.meta_path.append(path).append_extension(".meta");
    }

    const auto ext = thread.source_path.extension();
    int importer_index = get_default_importer_for_file_type(pipeline->import, ext);

    if (importer_index < 0)
    {
        return { AssetPipelineError::unsupported_file_type };
    }

    auto txn = g_assetdb.write(pipeline->import.db);

    AssetMetadata meta{};
    bool is_new_file = true;

    // If the file exists on disk - use it as source metadata info
    if (fs::is_file(thread.meta_path.view()))
    {
        auto file = fs::open_file(thread.meta_path.view(), fs::OpenMode::read);
        auto json = fs::read(file, temp_allocator());
        JSONSerializer serializer(json.data(), JSONSerializeFlags::parse_in_situ, temp_allocator());
        serialize(SerializerMode::reading, &serializer, &meta, temp_allocator());
        is_new_file = !g_assetdb.asset_exists(&txn, meta.guid);
        importer_index = find_index(pipeline->import.importer_hashes, meta.importer);

        if (importer_index < 0)
        {
            return { AssetPipelineError::importer_not_registerd };
        }
    }
    else
    {
        const auto settings_type = pipeline->import.importers[importer_index].importer->settings_type();
        meta.kind = AssetFileKind::file;
        meta.importer = pipeline->import.importer_hashes[importer_index];
        meta.settings = settings_type->create_instance(temp_allocator());
    }

    AssetInfo info;

    if (is_new_file)
    {
        auto res = g_assetdb.create_asset(&txn);
        if (!res)
        {
            return { AssetPipelineError::failed_to_create_asset };
        }

        info = *res.unwrap();
    }
    else
    {
        auto res = g_assetdb.get_asset_info(&txn, meta.guid);
        if (!res)
        {
            return { AssetPipelineError::failed_to_create_asset };
        }

        info = res.unwrap();
    }

    const u64 new_timestamp = fs::last_modified(thread.source_path.view());
    const u64 new_meta_timestamp = fs::last_modified(thread.meta_path.view());

    // if the timestamps are up to date and the meta file exists (i.e. hasn't been deleted for whatever reason)
    // then there's no need to re-import the asset as it hasn't been modified
    if (new_timestamp == info.timestamp && new_meta_timestamp == info.meta_timestamp && thread.meta_path.exists())
    {
        return {};
    }

    meta.guid = info.guid;

    info.importer = meta.importer;
    info.kind = meta.kind;
    info.timestamp = new_timestamp;
    info.meta_timestamp = new_meta_timestamp;

    auto res = g_assetdb.set_asset_path(&txn, meta.guid, thread.source_path.c_str());
    if (!res)
    {
        return { AssetPipelineError::failed_to_write_metadata };
    }

    res = g_assetdb.set_import_settings(&txn, meta.guid, meta.settings);
    if (!res)
    {
        return { AssetPipelineError::failed_to_write_metadata };
    }

    res = g_assetdb.remove_all_artifacts(&txn, meta.guid);
    if (!res)
    {
        return { AssetPipelineError::failed_to_write_artifacts };
    }

    res = g_assetdb.remove_all_dependencies(&txn, meta.guid);
    if (!res)
    {
        return { AssetPipelineError::failed_to_update_dependencies };
    }

    res = g_assetdb.remove_all_sub_assets(&txn, meta.guid);
    if (!res)
    {
        return { AssetPipelineError::failed_to_update_sub_assets };
    }

    AssetImportContext ctx{};
    ctx.temp_allocator = temp_allocator();
    ctx.target_platforms = AssetPlatform::windows | AssetPlatform::vulkan;
    ctx.guid = meta.guid;
    ctx.db = &g_assetdb;
    ctx.txn = &txn;
    ctx.artifact_buffer = &thread.artifact_buffer;
    ctx.path = thread.source_path.view();
    ctx.cache_root = pipeline->import.cache_path.view();
    ctx.importer_hash = meta.importer;
    ctx.settings = &meta.settings;

    enum_to_string(ctx.target_platforms, &thread.target_platform_string);
    str::replace(&thread.target_platform_string, " ", "");
    str::replace(&thread.target_platform_string, "|", "-");

    ctx.target_platform_string = thread.target_platform_string.view();

    auto& importer_info = pipeline->import.importers[importer_index];
    auto import_res = importer_info.importer->import(&ctx, importer_info.user_data);
    if (!import_res)
    {
        log_error("%s", import_res.unwrap_error().to_string());
        return { AssetPipelineError::failed_to_import };
    }

    JSONSerializer serializer(temp_allocator());
    serialize(SerializerMode::writing, &serializer, &meta, temp_allocator());
    {
        auto file = fs::open_file(thread.meta_path.view(), fs::OpenMode::write);
        if (!fs::write(file, serializer.c_str()))
        {
            return { AssetPipelineError::failed_to_write_metadata };
        }
    }

    // Set the metadata timestamp after writing the file to disk
    info.meta_timestamp = fs::last_modified(thread.meta_path.view());
    res = g_assetdb.set_asset_info(&txn, info);
    if (!res)
    {
        return { AssetPipelineError::failed_to_write_metadata };
    }

    txn.commit();

    log_info("Imported %s", thread.source_path.c_str());

    return {};
}

Result<AssetDatabase*, AssetPipelineError> get_asset_database(AssetPipeline* pipeline)
{
    if (!pipeline->can_import())
    {
        return { AssetPipelineError::import };
    }

    return pipeline->import.db;
}

static void import_assets_at_path(AssetPipeline* pipeline, const PathView& root)
{
    for (auto& entry : fs::read_dir(root))
    {
        if (fs::is_dir(entry))
        {
            import_assets_at_path(pipeline, entry);
            continue;
        }

        auto res = import_asset(pipeline, entry, AssetPlatform::unknown);
        if (!res)
        {
            log_error("%" BEE_PRIsv ": %s", BEE_FMT_SV(entry), res.unwrap_error().to_string());
        }
    }
}

void add_import_root(AssetPipeline* pipeline, const PathView& path)
{
    if (!pipeline->can_import())
    {
        return;
    }

    import_assets_at_path(pipeline, path);

    pipeline->import.source_watcher.suspend();
    pipeline->import.source_watcher.add_directory(path);
    pipeline->import.source_watcher.resume();
}

void remove_import_root(AssetPipeline* pipeline, const PathView& path)
{
    if (!pipeline->can_import())
    {
        return;
    }

    pipeline->import.source_watcher.suspend();
    pipeline->import.source_watcher.remove_directory(path);
    pipeline->import.source_watcher.resume();
}

Result<void, AssetPipelineError> refresh_import_pipeline(AssetPipeline* pipeline)
{
    pipeline->import.source_watcher.pop_events(&pipeline->import.source_events);

    for (auto& event : pipeline->import.source_events)
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

    g_assetdb.gc(pipeline->import.db);
    return {};
}

void set_import_pipeline(AssetPipelineModule* module, PluginLoader* loader, const PluginState state)
{
    module->register_importer = register_importer;
    module->unregister_importer = unregister_importer;
    module->import_asset = import_asset;
    module->get_asset_database = get_asset_database;
    module->add_import_root = add_import_root;
    module->remove_import_root = remove_import_root;
}


} // namespace bee