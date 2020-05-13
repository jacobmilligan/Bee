/*
 *  AssetPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Plugins/AssetPipeline/AssetPipeline.hpp"
#include "Bee/Plugins/AssetRegistry/AssetRegistry.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Jobs/JobDependencyCache.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"

#include <algorithm>

#define BEE_TESTING_ASSET_PIPELINE_INIT

namespace bee {


/*
 *********************************
 *
 * AssetDB forward declarations
 *
 *********************************
 */
extern AssetDatabaseModule g_assetdb_module;

void load_assetdb_module(bee::PluginRegistry* registry, const bee::PluginState state);

/*
 *********************************
 *
 * Asset Pipeline state
 *
 *********************************
 */

struct CompilerInfo
{
    u32                 hash { 0 };
    AssetCompiler*      compiler { nullptr };
    DynamicArray<u32>   extensions;
};

struct FileTypeMapping
{
    StaticString<32>    extension;
    DynamicArray<i32>   compiler_ids;
    DynamicArray<u32>   compiler_hashes;
};

struct AssetPipeline
{
    AssetPlatform                           platform { AssetPlatform::unknown };
    Path                                    project_root;
    Path                                    cache_root;
    fs::DirectoryWatcher                    asset_watcher { true }; // recursive
    DynamicArray<CompilerInfo>              compilers;
    DynamicHashMap<u32, FileTypeMapping>    filetype_map;
    FixedArray<AssetDbItem>                 per_thread_assetdb_items;
    JobDependencyCache                      job_deps;
};

static AssetPipeline* g_pipeline { nullptr };
static constexpr auto g_asset_ext = ".asset";

/*
 *********************************
 *
 * Asset compiler -> filetype
 * mappings
 *
 *********************************
 */
u32 get_extension_hash(const StringView& ext)
{
    return ext[0] == '.' ? get_hash(StringView(ext.data() + 1, ext.size() - 1)) : get_hash(ext);
}

i32 find_compiler(const u32 hash)
{
    return container_index_of(g_pipeline->compilers, [&](const CompilerInfo& info)
    {
        return info.hash == hash;
    });
}

i32 find_compilers_for_filetype(const StringView& extension, AssetCompiler** compilers)
{
    const auto ext_hash = get_extension_hash(extension);
    const auto ext_hash2 = get_extension_hash(".bsc");
    const auto* filetype = g_pipeline->filetype_map.find(ext_hash);

    if (filetype == nullptr)
    {
        return 0;
    }

    const auto count = filetype->value.compiler_ids.size();

    if (compilers != nullptr)
    {
        for (const auto& id : filetype->value.compiler_ids)
        {
            compilers[id] = g_pipeline->compilers[id].compiler;
        }
    }

    return count;
}

AssetDbItem& pipeline_local_db_item()
{
    return g_pipeline->per_thread_assetdb_items[get_local_job_worker_id()];
}


/*
 *********************************
 *
 * Asset Pipeline implementation
 *
 *********************************
 */
void refresh_directory(const Path& path);
void refresh_path(const Path& path);

#ifdef BEE_TESTING_ASSET_PIPELINE_INIT
void clean_asset_files(const Path& path)
{
    auto asset_file = path;
    asset_file.set_extension(g_asset_ext);

    if (asset_file.exists())
    {
        fs::remove(asset_file);
    }

    for (const auto& child : fs::read_dir(path))
    {
        if (fs::is_dir(child))
        {
            clean_asset_files(child);
            continue;
        }

        if (child.extension() == g_asset_ext)
        {
            fs::remove(child);
        }
    }
}
#endif // BEE_TESTING_ASSET_PIPELINE_INIT

bool init(const AssetPipelineInitInfo& info)
{
    BEE_ASSERT(info.platform != AssetPlatform::unknown);

    if (g_assetdb_module.is_open())
    {
        g_assetdb_module.close();
    }

    g_pipeline->platform = info.platform;
    g_pipeline->project_root = info.project_root;
    g_pipeline->cache_root = info.project_root.join(info.cache_directory);
    g_pipeline->per_thread_assetdb_items.resize(get_job_worker_count());

#ifdef BEE_TESTING_ASSET_PIPELINE_INIT
    if (g_pipeline->cache_root.exists())
    {
        fs::rmdir(g_pipeline->cache_root, true);
    }
#endif

    if (!g_pipeline->cache_root.exists())
    {
        fs::mkdir(g_pipeline->cache_root);
    }

    g_assetdb_module.open(g_pipeline->cache_root, info.asset_database_name);

    if (!g_assetdb_module.is_open())
    {
        return false;
    }

    // Add the builtin assets directory to be watched
    g_pipeline->asset_watcher.add_directory(fs::get_appdata().assets_root);

    for (const auto& dir : g_pipeline->asset_watcher.watched_directories())
    {
#ifdef BEE_TESTING_ASSET_PIPELINE_INIT
        clean_asset_files(dir);
#endif // BEE_TESTING_ASSET_PIPELINE_INIT
        refresh_path(dir);
    }

    g_pipeline->asset_watcher.start("AssetWatcher");
    return true;
}

void destroy()
{
    g_pipeline->job_deps.wait_all();

    if (g_pipeline->asset_watcher.is_running())
    {
        g_pipeline->asset_watcher.stop();
    }

    g_pipeline->compilers.clear();
    g_pipeline->filetype_map.clear();

    if (g_assetdb_module.is_open())
    {
        g_assetdb_module.close();
    }

    g_pipeline->platform = AssetPlatform::unknown;
}

void set_platform(const AssetPlatform platform)
{
    g_pipeline->job_deps.wait_all();
    g_pipeline->platform = platform;
}

struct ImportRequest
{
    Path                        src;
    Path                        dst;
    AssetPlatform               platform { AssetPlatform::unknown };
    String                      name;
    FixedArray<AssetCompiler*>  compilers;

    explicit ImportRequest(Allocator* allocator)
        : src(allocator),
          dst(allocator),
          name(allocator),
          compilers(allocator)
    {}
};

u64 write_asset_to_disk(const Path& dst, AssetFile* asset)
{
    asset->source.make_generic();

    JSONSerializer serializer(temp_allocator());
    serialize(SerializerMode::writing, &serializer, asset);
    fs::write(dst, serializer.c_str());

    return fs::last_modified(dst);
}

void read_asset_from_disk(const Path& src, AssetFile* asset)
{
    auto str = fs::read(src, temp_allocator());
    JSONSerializer serializer(str.data(), rapidjson::ParseFlag::kParseInsituFlag, temp_allocator());
    serialize(SerializerMode::reading, &serializer, asset);
}

u128 get_content_hash(const Path& src_path, const TypeInstance& options)
{
    static thread_local u8 buffer[4096];

    HashState128 hash;

    if (fs::is_file(src_path))
    {
        io::FileStream stream(src_path, "rb");

        while (stream.offset() < stream.size())
        {
            const auto read_size = math::min(static_array_length(buffer), stream.size() - stream.offset());

            stream.read(buffer, read_size);
            hash.add(buffer, read_size);
        }
    }
    else
    {
        hash.add(src_path.c_str(), src_path.size());
    }

    if (options.is_valid())
    {
        hash.add(options.data(), options.type()->size);
    }

    return hash.end();
}

void import_job(const ImportRequest& req)
{
    if (!req.src.exists())
    {
        log_error("Failed to import asset: %s is not a valid source path", req.src.c_str());
        return;
    }

    const auto dst_dir = req.dst.parent_path(temp_allocator());

    // Start the import
    if (!req.compilers.empty())
    {
        log_debug("Importing %s", req.src.c_str());
    }
    else
    {
        log_warning("Skipping import for %s: no compiler found for filetype \"%" BEE_PRIsv "\"", req.src.c_str(), BEE_FMT_SV(req.src.extension()));
    }

    if (!dst_dir.exists())
    {
        if (!fs::mkdir(dst_dir, true))
        {
            log_error("Failed to import %s: invalid dst path %s specified", req.dst.c_str(), req.src.c_str());
            return;
        }
    }

    auto& db_item = pipeline_local_db_item();
    auto& asset = db_item.contents;

    if (!req.compilers.empty())
    {
        asset.options = req.compilers[0]->options_type()->create_instance(temp_allocator());
    }

    asset.content_hash = get_content_hash(req.src, asset.options);

    // use the existing .asset file as source of guid etc. if one exists
    if (req.dst.exists())
    {
        read_asset_from_disk(req.dst, &asset);
    }
    else
    {
        // otherwise we're importing a brand new asset
        asset.guid = generate_guid();
        asset.source = req.src.relative_to(g_pipeline->project_root, temp_allocator());
    }

    // Assign the new name if one was requested
    if (req.name != asset.name)
    {
        asset.name.clear(); // reuse the existing memory
        asset.name.append(req.name);
    }

    DynamicArray<AssetArtifact> artifacts(temp_allocator());
    auto status = AssetCompilerStatus::unknown;

    // Compile the asset! Multiple compilers may know about this filetype, so iterate over them all
    for (const auto& compiler : req.compilers)
    {
        AssetCompilerContext ctx(
            req.platform,
            req.src.view(),
            g_pipeline->cache_root.view(),
            asset.options,
            &artifacts,
            temp_allocator()
        );

        status = compiler->compile(compiler->data, get_local_job_worker_id(), &ctx);

        if (status != AssetCompilerStatus::success)
        {
            log_error("Failed to import asset %s: %s", req.src.c_str(), enum_to_type(status).name);
            return;
        }
    }

    // Calculate hashes and sort artifacts
    for (auto& artifact : artifacts)
    {
        artifact.hash = get_hash128(artifact.buffer.data(), artifact.buffer.size(), 0xF00D);
    }

    std::sort(artifacts.begin(), artifacts.end(), [&](const AssetArtifact& lhs, const AssetArtifact& rhs)
    {
        return lhs.hash < rhs.hash;
    });

    auto& assetdb = g_assetdb_module;
    auto txn = assetdb.write();

    /*
     * If the set of compilers for this asset type has changed there may be a different set of artifacts
     * produced - check all the artifacts and delete any that were in the existing asset but not produced by
     * this import
     */
    for (auto& hash : asset.artifacts)
    {
        const auto index = container_index_of(artifacts, [&](const AssetArtifact& artifact)
        {
            return artifact.hash == hash;
        });

        if (index < 0)
        {
            assetdb.delete_artifact(txn, hash);
        }
    }

    asset.artifacts.clear();

    // Put the new artifacts into the DB
    for (const auto& artifact : artifacts)
    {
        assetdb.put_artifact(txn, artifact);
        asset.artifacts.push_back(artifact.hash);
    }

    /*
     * Persist the json .asset file to disk then update it in the DB. We have to do it in this order
     * (write to disk, then update in DB) to ensure we have an up-to-date file timestamp to put into
     * the DB
     */
    db_item.src_timestamp = fs::last_modified(req.src);
    db_item.dst_timestamp = write_asset_to_disk(req.dst, &asset);

    assetdb.put_asset(txn, &db_item);
    assetdb.commit_transaction(&txn);
}


void import_asset(const Path& source_path, const Path& dst_path, const StringView& name)
{
    // import as asset
    ImportRequest req(temp_allocator());
    req.platform = g_pipeline->platform;
    req.name.append(name);
    req.dst.append(dst_path);
    req.src.append(source_path);

    const auto compiler_count = find_compilers_for_filetype(source_path.extension(), nullptr);
    req.compilers.resize(compiler_count);
    find_compilers_for_filetype(source_path.extension(), req.compilers.data());

    auto* job = create_job(import_job, std::move(req));
    g_pipeline->job_deps.write(source_path, job);
}

void delete_asset(const GUID& guid, const DeleteAssetKind kind)
{
    log_debug("Deleting asset %s", format_guid(guid, GUIDFormat::digits));

    auto& assetdb = g_assetdb_module;
    auto txn = assetdb.write();

    auto& db_item = pipeline_local_db_item();
    if (!assetdb.get_asset(txn, guid, &db_item))
    {
        log_error("Failed to delete asset");
        return;
    }

    auto& asset = db_item.contents;

    const auto src_path = g_pipeline->project_root.join(asset.source);

    g_pipeline->job_deps.write(src_path, create_null_job());

    // Delete from disk if needed
    if (kind == DeleteAssetKind::asset_and_source)
    {
        if (!src_path.exists())
        {
            log_error("Failed to delete asset: invalid source path %s", src_path.c_str());
            return;
        }

        if (BEE_FAIL(fs::remove(src_path)))
        {
            return;
        }
    }

    // Delete from database
    assetdb.delete_asset(txn, guid);
    assetdb.commit_transaction(&txn);

    // Delete .asset file
    const auto asset_file_path = g_pipeline->project_root.join(db_item.contents.source).set_extension(g_asset_ext);

    if (asset_file_path.exists() && !fs::remove(asset_file_path))
    {
        log_error("Failed to delete asset: invalid .asset file location %s", asset_file_path.c_str());
    }
}

void delete_asset_at_path(const Path& path, const DeleteAssetKind kind)
{
    auto& db_item = pipeline_local_db_item();
    auto txn = g_assetdb_module.read();

    if (!g_assetdb_module.get_asset_from_path(txn, path, &db_item))
    {
        return;
    }

    g_assetdb_module.commit_transaction(&txn);

    delete_asset(db_item.contents.guid, kind);
}

void delete_asset_with_name(const StringView& name, const DeleteAssetKind kind)
{
    auto& assetdb = g_assetdb_module;
    auto txn = assetdb.read();

    GUID guid{};
    if (!assetdb.get_guid_from_name(txn, name, &guid))
    {
        log_error("Failed to delete asset: no GUID found for asset with name \"%" BEE_PRIsv "\"", BEE_FMT_SV(name));
        return;
    }

    assetdb.commit_transaction(&txn);

    delete_asset(guid, kind);
}

void register_compiler(AssetCompiler* compiler)
{
    const auto* name = compiler->get_name();
    const auto hash = get_hash(name);

    if (find_compiler(hash) >= 0)
    {
        log_error("Asset compiler \"%s\" is already registered", name);
        return;
    }

    g_pipeline->compilers.emplace_back();

    auto& info = g_pipeline->compilers.back();
    info.hash = hash;
    info.compiler = compiler;

    const auto compiler_id = g_pipeline->compilers.size() - 1;

    // Validate that no compilers have been registered with the supported extensions
    const auto supported_filetypes = compiler->supported_file_types();

    for (const auto* ext : supported_filetypes)
    {
        const auto ext_hash = get_extension_hash(ext);

        if (container_index_of(info.extensions, [&](const u32 hash) { return hash == ext_hash; }) >= 0)
        {
            log_warning("Asset compiler \"%s\" defines the same file extension (%s) multiple times", name, ext);
            continue;
        }

        auto* filetype_mapping = g_pipeline->filetype_map.find(ext_hash);
        if (filetype_mapping == nullptr)
        {
            filetype_mapping = g_pipeline->filetype_map.insert(ext_hash, FileTypeMapping());
            filetype_mapping->value.extension = ext;
        }

        filetype_mapping->value.compiler_ids.push_back(compiler_id);
        filetype_mapping->value.compiler_hashes.push_back(hash);

        info.extensions.push_back(ext_hash);
    }

    info.compiler->init(info.compiler->data, get_job_worker_count());

    // TODO(Jacob): iterate watched directories and import all assets with a filetype in supported_filetypes that
    //  haven't already been imported OR have a newer timestamp than what's stored in the AssetDB
}

void unregister_compiler(AssetCompiler* compiler)
{
    const auto* name = compiler->get_name();
    const auto hash = get_hash(name);
    auto id = find_compiler(hash);

    if (BEE_FAIL_F(id >= 0, "Cannot unregister asset compiler: no compiler registered with name \"%s\"", name))
    {
        return;
    }

    for (const auto ext_hash : g_pipeline->compilers[id].extensions)
    {
        auto* extension_mapping = g_pipeline->filetype_map.find(ext_hash);
        if (extension_mapping != nullptr)
        {
            const auto compiler_mapping_idx = container_index_of(extension_mapping->value.compiler_ids, [&](const i32 stored)
            {
                return stored == id;
            });

            if (compiler_mapping_idx >= 0)
            {
                extension_mapping->value.compiler_ids.erase(compiler_mapping_idx);
                extension_mapping->value.compiler_hashes.erase(compiler_mapping_idx);

                if (extension_mapping->value.compiler_ids.empty())
                {
                    g_pipeline->filetype_map.erase(ext_hash);
                }
            }
        }
    }

    g_pipeline->compilers[id].compiler->destroy(g_pipeline->compilers[id].compiler->data);
    g_pipeline->compilers.erase(id);
}

void add_asset_directory(const Path& path)
{
    if (!path.exists())
    {
        log_error("The asset root path does not exist");
        return;
    }

    g_pipeline->asset_watcher.add_directory(path);
}

void remove_asset_directory(const Path& path)
{
    g_pipeline->asset_watcher.remove_directory(path);
}

Span<const Path> asset_directories()
{
    return g_pipeline->asset_watcher.watched_directories();
}

void refresh_path(const Path& path)
{
    Path src_path(path.view(), temp_allocator());

    const auto is_asset_file = path.extension() == g_asset_ext;

    AssetFile disk_asset(temp_allocator());

    // Get the source path from the .asset file at path instead
    if (is_asset_file)
    {
        src_path.clear();

        if (path.exists())
        {
            read_asset_from_disk(path, &disk_asset);
            src_path.append(g_pipeline->project_root).append(disk_asset.source).normalize();
        }
        else
        {
            src_path.append(path.view()).set_extension("");
        }
    }

    g_pipeline->job_deps.read(src_path, create_null_job());

    const auto stored_path = src_path.relative_to(g_pipeline->project_root, temp_allocator());

    // Check if we've already imported the source file
    auto txn = g_assetdb_module.read();

    auto& db_item = pipeline_local_db_item();
    auto& stored_asset = db_item.contents;

    const auto is_reimport = g_assetdb_module.get_asset_from_path(txn, stored_path, &db_item);

    g_assetdb_module.commit_transaction(&txn);

    // Get the .asset files full path if `path` isn't already an asset file
    Path asset_file_path(path.view(), temp_allocator());
    if (!is_asset_file)
    {
        asset_file_path.append_extension(g_asset_ext);
    }

    // if the source path is missing, delete the asset from the DB
    if (!src_path.exists())
    {
        delete_asset_at_path(stored_path, DeleteAssetKind::asset_only);
        return;
    }

    // Check timestamps as a quick first change test
    const auto disk_asset_exists = asset_file_path.exists();
    const auto src_timestamp = fs::last_modified(src_path);
    const auto asset_file_timestamp = disk_asset_exists ? fs::last_modified(asset_file_path) : 0;

    if (is_reimport)
    {
        if (!disk_asset_exists)
        {
            delete_asset_at_path(stored_path, DeleteAssetKind::asset_only);
            return;
        }

        if (src_timestamp == db_item.src_timestamp && asset_file_timestamp == db_item.dst_timestamp)
        {
            return;
        }
    }

    BEE_ASSERT(src_path.extension() != g_asset_ext);

    if (!is_asset_file && disk_asset_exists)
    {
        read_asset_from_disk(asset_file_path, &disk_asset);
    }

    if (is_reimport)
    {
        // Check content hash as a full change test
        const auto content_hash = get_content_hash(src_path, disk_asset.options);

        // same content hashes - no change
        if (content_hash == stored_asset.content_hash)
        {
            return;
        }

        stored_asset = std::move(disk_asset);
        stored_asset.content_hash = content_hash;
    }
    else
    {
        stored_asset.content_hash = get_content_hash(src_path, disk_asset.options);
    }

    // disk_asset may have been moved into stored_asset here - DO NOT USE IT - only use stored_asset

    if (fs::is_file(src_path))
    {
        import_asset(src_path, asset_file_path, stored_asset.name.view());
    }
    else
    {
        stored_asset.is_directory = true;

        if (!is_reimport)
        {
            stored_asset.guid = generate_guid();
            stored_asset.source.clear();
            stored_asset.source.append(stored_path.view());
            stored_asset.artifacts.clear();
            stored_asset.name.clear();
        }

        db_item.src_timestamp = src_timestamp;
        db_item.dst_timestamp = write_asset_to_disk(asset_file_path, &stored_asset);

        log_debug("%s directory %s", is_reimport ? "Reimporting" : "Importing", src_path.c_str());

        txn = g_assetdb_module.write();
        g_assetdb_module.put_asset(txn, &db_item);
        g_assetdb_module.commit_transaction(&txn);

        if (!is_asset_file)
        {
            for (const auto& child : fs::read_dir(src_path))
            {
                refresh_path(child);
            }
        }
    }
}

void refresh()
{
    auto events = g_pipeline->asset_watcher.pop_events();

    for (auto& event : events)
    {
        refresh_path(event.file);
    }

    g_pipeline->job_deps.trim();
}


/*
 * Locator
 */
bool locate_asset_by_name(const StringView& name, GUID* guid, AssetLocation* location)
{
    auto txn = g_assetdb_module.read();
    if (!g_assetdb_module.get_guid_from_name(txn, name, guid))
    {
        return false;
    }

    location->path =
}

static AssetLocator g_assetdb_locator{};

void asset_registry_observer(void* module, void* user_data)
{
    g_assetdb_locator.locate_by_name()
}


} // namespace bee


static bee::AssetPipelineModule g_module{};

BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    load_assetdb_module(registry, state);

    bee::g_pipeline = registry->get_or_create_persistent<bee::AssetPipeline>("BeeAssetPipeline");

    g_module.init = bee::init;
    g_module.destroy = bee::destroy;
    g_module.set_platform = bee::set_platform;
    g_module.import_asset = bee::import_asset;
    g_module.delete_asset = bee::delete_asset;
    g_module.delete_asset_with_name = bee::delete_asset_with_name;
    g_module.register_compiler = bee::register_compiler;
    g_module.unregister_compiler = bee::unregister_compiler;
    g_module.add_asset_directory = bee::add_asset_directory;
    g_module.remove_asset_directory = bee::remove_asset_directory;
    g_module.asset_directories = bee::asset_directories;
    g_module.refresh = bee::refresh;

    registry->toggle_module(state, BEE_ASSET_PIPELINE_MODULE_NAME, &g_module);
}