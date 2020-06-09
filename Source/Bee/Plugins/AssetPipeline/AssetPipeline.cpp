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
#include "Bee/Core/Serialization/BinarySerializer.hpp"

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
extern AssetDatabaseModule g_assetdb;

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
    AssetCompilerId     id { 0 };
    AssetCompilerOrder  order { AssetCompilerOrder::first };
    u32                 type_hash { 0 };
    AssetCompiler*      instance { nullptr };
    DynamicArray<u32>   extensions;
};

struct FileTypeMapping
{
    StaticString<32>    extension;
    DynamicArray<i32>   compiler_ids;
    DynamicArray<u32>   compiler_hashes;
};

struct AssetLocatorInstance
{
    AssetPipeline* pipeline { nullptr };
};

struct ImportRequest
{
    JobGroup*           wait_handle { nullptr };
    AssetCompilerOrder  order { AssetCompilerOrder::none };
    StaticString<1024>  uri;
    AssetPlatform       platform { AssetPlatform::unknown };
    AssetCompilerId     compiler_id;
    AssetCompiler*      compiler { nullptr };
    AssetPipeline*      pipeline { nullptr };
};

BEE_VERSIONED_HANDLE_32(ImportBatchHandle);

struct AssetImportBatch
{
    AssetPipeline*              pipeline { nullptr };
    DynamicArray<ImportRequest> requests;
    DynamicArray<JobGroup>      groups;
    JobGroup                    wait_handle;

    explicit AssetImportBatch(AssetPipeline* pipeline_instance);

    void reset();

    bool add(const Path& source_path, const AssetCompilerId& compiler_id);
};

struct AssetPipeline
{
    struct ThreadData
    {
        CompiledAsset   asset;
        String          uri;
        Path            path;
    };

    Allocator*                  allocator { nullptr };
    RecursiveMutex              mutex;
    AssetPlatform               platform { AssetPlatform::unknown };
    Path                        project_root;
    Path                        cache_root;
    Path                        saved_location;
    fs::DirectoryWatcher        asset_watcher { true }; // recursive
    FixedArray<ThreadData>      thread_data;

    // Asset Database
    AssetDatabaseEnv*           db {nullptr };
    AssetLocatorInstance        locator_instance;
    AssetLocator                locator;

    // Asset importing
    AtomicStack                 free_batches;
    AtomicStack                 scheduled_batches;
};

struct GlobalAssetPipeline
{
    JobDependencyCache                      asset_op_deps;
    DynamicArray<CompilerInfo>              compilers;
    DynamicHashMap<u32, FileTypeMapping>    filetype_map;
    DynamicArray<AssetPipeline*>            all_pipelines;
};

static GlobalAssetPipeline*     g_pipeline { nullptr };
static PluginRegistry*          g_plugin_registry { nullptr };
static AssetRegistryModule*     g_asset_registry { nullptr };
static bee::AssetPipelineModule g_module{};

static constexpr auto g_metadata_ext = ".meta";

AssetPipeline::ThreadData& get_thread_data(AssetPipeline* pipeline)
{
    return pipeline->thread_data[get_local_job_worker_id()];
}

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
    return find_index_if(g_pipeline->compilers, [&](const CompilerInfo& info)
    {
        return info.id.id == hash;
    });
}

i32 find_compiler(const AssetCompilerId& id)
{
    return find_index_if(g_pipeline->compilers, [&](const CompilerInfo& info)
    {
        return info.id == id;
    });
}

i32 get_compilers_for_filetype(const StringView& extension, AssetCompilerId* dst_buffer)
{
    const auto ext_hash = get_extension_hash(extension);
    const auto* filetype = g_pipeline->filetype_map.find(ext_hash);

    if (filetype == nullptr)
    {
        return 0;
    }

    const auto count = filetype->value.compiler_ids.size();

    if (dst_buffer != nullptr)
    {
        for (const auto hash : enumerate(filetype->value.compiler_hashes))
        {
            dst_buffer[hash.index].id = hash.value;
        }
    }

    return count;
}

i32 find_default_compiler_for_filetype(const StringView& extension)
{
    const auto ext_hash = get_extension_hash(extension);
    const auto* filetype = g_pipeline->filetype_map.find(ext_hash);

    if (filetype == nullptr || filetype->value.compiler_ids.empty())
    {
        return -1;
    }

    const auto index = filetype->value.compiler_ids[0];
    return index;
}

u64 write_metadata(const Path& dst, AssetMetadata* meta)
{
    JSONSerializer serializer(temp_allocator());
    serialize(SerializerMode::writing, &serializer, meta);
    fs::write(dst, serializer.c_str());

    return fs::last_modified(dst);
}

void read_metadata(const Path& src, AssetMetadata* meta)
{
    auto str = fs::read(src, temp_allocator());
    JSONSerializer serializer(str.data(), rapidjson::ParseFlag::kParseInsituFlag, temp_allocator());
    serialize(SerializerMode::reading, &serializer, meta);
}

u128 get_source_hash(const Path& src_path, const TypeInstance& settings)
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

    if (settings.is_valid())
    {
        hash.add(settings.data(), settings.type()->size);
    }

    return hash.end();
}

u128 get_content_hash(const AssetPlatform platform, const DynamicArray<u8>& data)
{
    HashState128 hash;
    hash.add(platform);
    hash.add(data.data(), data.size());
    return hash.end();
}

void asset_path_to_uri(AssetPipeline* instance, const Path& src, String* dst)
{
    const auto is_builtin = src.is_relative_to(fs::get_root_dirs().install_root);
    const auto& root = is_builtin ? fs::get_root_dirs().install_root : instance->project_root;
    const char* scheme = is_builtin ? "builtin:/" : "project:/";

    dst->clear();
    dst->append(scheme);
    auto root_begin = root.begin();
    auto src_begin = src.begin();

    while (root_begin != root.end())
    {
        ++root_begin;
        ++src_begin;
    }

    while (src_begin != src.end())
    {
        *dst += Path::generic_slash;
        *dst += *src_begin;
        ++src_begin;
    }
}

void asset_uri_to_path(AssetPipeline* instance, const StringView& uri, Path* dst)
{
    dst->clear();

    const auto scheme_separator = str::first_index_of(uri, "://");

    if (BEE_FAIL(scheme_separator >= 0))
    {
        return;
    }

    const auto scheme_name = str::substring(uri, 0, scheme_separator);
    const auto filepath = str::substring(uri, scheme_separator + 3);

    if (scheme_name == "builtin")
    {
        dst->append(fs::get_root_dirs().install_root).append(filepath);
    }
    else
    {
        dst->append(instance->project_root).append(filepath);
    }
}

CompiledAsset& get_temp_asset(AssetPipeline* instance)
{
    auto& asset = get_thread_data(instance).asset;
    asset.src_timestamp = 0;
    asset.metadata_timestamp = 0;
    asset.source_hash = u128{};
    asset.main_artifact = AssetArtifact{};
    asset.uri.clear();
    new (&asset.metadata) AssetMetadata{};
    return asset;
}


/*
 *********************************
 *
 * Asset Pipeline fwd
 *
 *********************************
 */
void plugin_observer(const PluginEventType event, const bee::PluginDescriptor& plugin, const StringView& module_name, void* module, void* user_data);

void serialize_manifests(const SerializerMode mode, AssetPipeline* instance);

void load_manifests_at_path(AssetPipeline* instance, const Path& path);

void refresh_directory(AssetPipeline* instance, const Path& path);

void refresh_path(AssetImportBatch* batch, const Path& path);

void destroy(AssetPipeline* instance);

const char* get_runtime_locator_name();

bool runtime_locate_asset(AssetLocatorInstance* instance, const GUID& guid, AssetLocation* location);

AssetImportBatch* create_import_batch(AssetPipeline* instance);

void schedule_import_batch(AssetImportBatch* batch);


/*
 *********************************
 *
 * ImportBatch and import job
 * scheduling
 *
 *********************************
 */
void import_job(const ImportRequest* req);

void schedule_batch_job(AssetImportBatch* batch);

AssetImportBatch::AssetImportBatch(AssetPipeline* pipeline_instance)
    : pipeline(pipeline_instance)
{}

void AssetImportBatch::reset()
{
    requests.clear();
    groups.clear();
}

bool AssetImportBatch::add(const Path& source_path, const AssetCompilerId &compiler_id)
{
    if (!source_path.exists())
    {
        log_error("Failed to import asset: %s is not a valid source path", source_path.c_str());
        return false;
    }

    auto& uri = get_thread_data(pipeline).uri;
    asset_path_to_uri(pipeline, source_path, &uri);

    int compiler_index = -1;
    const auto ext = source_path.extension();

    if (compiler_id.is_valid())
    {
        compiler_index = find_compiler(compiler_id);
    }
    else
    {
        compiler_index = find_default_compiler_for_filetype(source_path.extension());

        if (compiler_index < 0)
        {
            log_warning(
                "Failed to import %s: no registered compiler supports \"%" BEE_PRIsv "\" files",
                uri.c_str(),
                BEE_FMT_SV(ext)
            );

            return false;
        }
    }

    if (compiler_index < 0)
    {
        log_warning("Skipping import for %s: no compiler registered with id \"%" PRIu32 "\"", uri.c_str(), compiler_id.id);
        return false;
    }

    auto& compiler = g_pipeline->compilers[compiler_index];

    const auto ext_hash = get_extension_hash(ext);

    if (find_index(compiler.extensions, ext_hash) < 0)
    {
        log_error(
            "Failed to import %s: compiler \"%s\" does not support \"%" BEE_PRIsv "\" files",
            uri.c_str(),
            compiler.instance->get_name(),
            BEE_FMT_SV(ext)
        );

        return false;
    }

    // import as asset
    requests.emplace_back();

    auto& req = requests.back();
    req.pipeline = pipeline;
    req.compiler_id = compiler.id;
    req.compiler = compiler.instance;
    req.order = compiler.order;
    req.platform = pipeline->platform;
    req.uri = uri.view();

    return true;
}

void import_batch_job(AssetImportBatch* batch)
{
    std::sort(batch->requests.begin(), batch->requests.end(), [](const ImportRequest& lhs, const ImportRequest& rhs)
    {
        return lhs.order < rhs.order;
    });

    auto ordering_dep = AssetCompilerOrder::none;
    batch->groups.clear();
    batch->groups.emplace_back();

    for (auto& req : batch->requests)
    {
        if (req.order != ordering_dep)
        {
            batch->groups.emplace_back();
            ordering_dep = req.order;
        }

        // Get hash here because req is moved into the job when creating it
        auto* job = create_job(import_job, &req);
        const auto uri_hash = get_hash(req.uri);

        if (batch->groups.size() > 1)
        {
            req.wait_handle = &batch->groups[batch->groups.size() - 2];
        }

        g_pipeline->asset_op_deps.schedule_write(uri_hash, job, &batch->groups.back());
    }

    for (auto& group : batch->groups)
    {
        job_wait(&group);
    }

    batch->reset();
    batch->pipeline->free_batches.push(atomic_node_cast(batch));
}

void import_job(const ImportRequest* req)
{
    if (req->wait_handle != nullptr)
    {
        job_wait(req->wait_handle);
    }

    // Setup the compiled asset data
    auto* pipeline = req->pipeline;
    auto& asset = get_temp_asset(pipeline);
    auto& metadata = asset.metadata;

    // Setup the uri
    asset.uri.append(req->uri.view());

    // Start the import
    auto& full_path = get_thread_data(req->pipeline).path;
    asset_uri_to_path(req->pipeline, req->uri.view(), &full_path);

    Path metadata_path(full_path.view(), temp_allocator());
    metadata_path.append_extension(g_metadata_ext);

    const auto dst_dir = metadata_path.parent_path(temp_allocator());

    if (!dst_dir.exists())
    {
        if (!fs::mkdir(dst_dir, true))
        {
            log_error("Failed to import %s: invalid dst path %s specified", metadata_path.c_str(), asset.uri.c_str());
            return;
        }
    }

    // use the existing .meta file as source of guid etc. if one exists
    if (metadata_path.exists())
    {
        read_metadata(metadata_path, &metadata);
    }
    else
    {
        // otherwise we're importing a brand new asset
        metadata.guid = generate_guid();
        metadata.is_directory = false;
        if (req->compiler->settings_type != nullptr)
        {
            metadata.settings = req->compiler->settings_type()->create_instance(temp_allocator());
        }
    }

    metadata.compiler = req->compiler_id;

    asset.src_timestamp = fs::last_modified(full_path);
    asset.source_hash = get_source_hash(full_path, metadata.settings);

    DynamicArray<TypeRef> artifact_types(temp_allocator());
    DynamicArray<DynamicArray<u8>> artifact_buffers(temp_allocator());
    DynamicArray<GUID> dependencies(temp_allocator());

    AssetCompilerOutput results{};
    results.artifact_buffers = &artifact_buffers;
    results.artifact_types = &artifact_types;
    results.dependencies = &dependencies;

    // Compile the asset!
    AssetCompilerContext ctx(
        &g_assetdb,
        req->pipeline->db,
        req->platform,
        full_path.view(),
        pipeline->cache_root.view(),
        metadata.settings,
        results,
        temp_allocator()
    );

    const auto status = req->compiler->compile(req->compiler->data, get_local_job_worker_id(), &ctx);

    if (status != AssetCompilerStatus::success)
    {
        log_error("Failed to import asset %s: %s", asset.uri.c_str(), enum_to_type(status).name);
        return;
    }

    if (ctx.main_artifact() < 0)
    {
        log_error("Failed to import asset %s: no main artifact was set by compiler \"%s\"", asset.uri.c_str(), req->compiler->get_name());
        return;
    }

    // Open a read transaction
    auto txn = g_assetdb.write(pipeline->db);

    AssetArtifact artifact{};

    // Calculate hashes and put the artifacts into the DB - this is sorted internally
    for (int i = 0; i < artifact_buffers.size(); ++i)
    {
        artifact.type_hash = artifact_types[i]->hash;
        artifact.content_hash = get_content_hash(req->platform, artifact_buffers[i]);

        // ensure we keep track of the content hash for the main asset
        if (i == ctx.main_artifact())
        {
            asset.main_artifact = artifact;
        }

        if (!g_assetdb.put_artifact(pipeline->db, txn, metadata.guid, artifact, artifact_buffers[i].data(), artifact_buffers[i].size()))
        {
            log_error("Failed to save asset artifact data for %s", asset.uri.c_str());
            return;
        }
    }

    // Set the GUID dependencies for the asset
    if (!g_assetdb.set_asset_dependencies(pipeline->db, txn, metadata.guid, dependencies.data(), dependencies.size()))
    {
        log_error("Failed to write dependency information for %s", asset.uri.c_str());
        return;
    }

    // write the final asset information to the database
    if (!g_assetdb.put_asset(pipeline->db, txn, metadata.guid, &asset))
    {
        log_error("Failed to update asset in database");
        return;
    }

    /*
     * Persist the json .meta file to disk then update it in the DB. We have to do it in this order
     * (write to disk, then update in DB) to ensure we have an up-to-date file timestamp to put into
     * the DB
     */
    asset.metadata_timestamp = write_metadata(metadata_path, &metadata);

    g_assetdb.commit(pipeline->db, &txn);

    log_debug("Imported %s", asset.uri.c_str());
}

/*
 *********************************
 *
 * Asset Pipeline utils
 *
 *********************************
 */
#ifdef BEE_TESTING_ASSET_PIPELINE_INIT
void clean_asset_files(const Path& path)
{
    auto asset_file = path;
    asset_file.set_extension(g_metadata_ext);

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

        if (child.extension() == g_metadata_ext)
        {
            fs::remove(child);
        }
    }
}
#endif // BEE_TESTING_ASSET_PIPELINE_INIT


/*
 *********************************
 *
 * Asset Pipeline implementation
 *
 *********************************
 */
AssetPipeline* init(const AssetPipelineInitInfo& info, Allocator* allocator)
{
    BEE_ASSERT(info.platform != AssetPlatform::unknown);

    auto* instance = BEE_NEW(allocator, AssetPipeline);
    instance->allocator = allocator;
    instance->platform = info.platform;
    instance->project_root = info.project_root;
    instance->cache_root = info.project_root.join(info.cache_directory);
    instance->thread_data.resize(get_job_worker_count());

#ifdef BEE_TESTING_ASSET_PIPELINE_INIT
    if (instance->cache_root.exists())
    {
        fs::rmdir(instance->cache_root, true);
    }
#endif

    if (!instance->cache_root.exists())
    {
        fs::mkdir(instance->cache_root);
    }

    // Load up the asset pipeline manifests
    instance->saved_location = instance->cache_root.join("Manifests");
    serialize_manifests(SerializerMode::reading, instance);

    // Load up the assetdatabase
    instance->db = g_assetdb.open(instance->cache_root, info.asset_database_name, allocator);

    if (!g_assetdb.is_open(instance->db))
    {
        destroy(instance);
        return nullptr;
    }

    // Add all the subdirectories under Assets/ in the project root as asset directories
    for (const auto& dir : fs::read_dir(fs::get_root_dirs().assets_root))
    {
        if (!fs::is_dir(dir))
        {
            continue;
        }
        instance->asset_watcher.add_directory(dir);
    }

    g_plugin_registry->add_observer(plugin_observer, instance);

    // Initialize the locator with the right function & instance data
    instance->locator_instance.pipeline = instance;
    instance->locator.instance = &instance->locator_instance;
    instance->locator.locate = runtime_locate_asset;
    instance->locator.get_name = get_runtime_locator_name;

    g_asset_registry->add_locator(&instance->locator);
    g_pipeline->all_pipelines.push_back(instance);

    // Refresh all assets
    if (!instance->asset_watcher.watched_directories().empty())
    {
        auto* batch = create_import_batch(instance);

        for (const auto& dir : instance->asset_watcher.watched_directories())
        {
#ifdef BEE_TESTING_ASSET_PIPELINE_INIT
            clean_asset_files(dir);
#endif // BEE_TESTING_ASSET_PIPELINE_INIT
            refresh_path(batch, dir);
        }

        schedule_import_batch(batch);
    }

    // Load any roots from plugins that were loaded before the asset pipeline was
    const auto loaded_plugin_count = g_plugin_registry->get_loaded_plugins(nullptr);
    auto plugin_descs = FixedArray<PluginDescriptor>::with_size(loaded_plugin_count, temp_allocator());
    g_plugin_registry->get_loaded_plugins(plugin_descs.data());

    Path plugin_path(temp_allocator());
    for (auto& desc : plugin_descs)
    {
        desc.get_full_path(&plugin_path);
        load_manifests_at_path(instance, plugin_path);
    }

    // init the asset directory watcher
    instance->asset_watcher.start("AssetWatcher");

    return instance;
}

void destroy(AssetPipeline* instance)
{
    g_pipeline->asset_op_deps.wait_all();

    const auto index = find_index(g_pipeline->all_pipelines, instance);
    BEE_ASSERT(index >= 0);
    g_pipeline->all_pipelines.erase(index);

    g_asset_registry->remove_locator(&instance->locator);
    g_plugin_registry->remove_observer(plugin_observer, instance);

    if (instance->asset_watcher.is_running())
    {
        instance->asset_watcher.stop();
    }

    g_pipeline->compilers.clear();

    if (g_assetdb.is_open(instance->db))
    {
        g_assetdb.close(instance->db);
    }

    instance->platform = AssetPlatform::unknown;

    BEE_DELETE(instance->allocator, instance);
}

void serialize_manifests(const SerializerMode mode, AssetPipeline* instance)
{
    if (mode == SerializerMode::reading && !instance->saved_location.exists())
    {
        return;
    }

    const auto* open_mode = mode == SerializerMode::reading ? "rb": "wb";
    io::FileStream stream(instance->saved_location, open_mode);
    g_asset_registry->serialize_manifests(mode, &stream);
}

void set_platform(AssetPipeline* instance, const AssetPlatform platform)
{
    g_pipeline->asset_op_deps.wait_all();
    instance->platform = platform;
}

AssetImportBatch* create_import_batch(AssetPipeline* instance)
{
    auto* node = instance->free_batches.pop();
    if (node != nullptr)
    {
        return static_cast<AssetImportBatch*>(node->data[0]);
    }

    return make_atomic_node<AssetImportBatch>(system_allocator(), instance).data;
}

void schedule_import_batch(AssetImportBatch* batch)
{
    auto* job = create_job(import_batch_job, batch);
    job_schedule(&batch->wait_handle, job);
    batch->pipeline->scheduled_batches.push(atomic_node_cast(batch));
}

void import_asset(AssetImportBatch* batch, const Path& source_path)
{
    auto& asset = get_temp_asset(batch->pipeline);

    auto txn = g_assetdb.read(batch->pipeline->db);
    auto& uri = get_thread_data(batch->pipeline).uri;
    asset_path_to_uri(batch->pipeline, source_path, &uri);
    const auto reimport = g_assetdb.get_asset_from_path(batch->pipeline->db, txn, uri.view(), &asset);
    g_assetdb.commit(batch->pipeline->db, &txn);

    if (reimport)
    {
        batch->add(source_path, asset.metadata.compiler);
        return;
    }

    const auto meta_path = Path(source_path.view(), temp_allocator()).append_extension(g_metadata_ext);

    if (!meta_path.exists())
    {
        batch->add(source_path, AssetCompilerId{});
    }
    else
    {
        AssetMetadata meta{};
        read_metadata(source_path, &meta);
        batch->add(source_path, meta.compiler);
    }
}

void delete_asset(AssetPipeline* instance, const GUID& guid, const DeleteAssetKind kind)
{
    log_debug("Deleting asset %s", format_guid(guid, GUIDFormat::digits));

    auto txn = g_assetdb.write(instance->db);

    auto& asset = get_temp_asset(instance);
    if (!g_assetdb.get_asset(instance->db, txn, guid, &asset))
    {
        log_error("Failed to delete asset");
        return;
    }

    auto& src_path = get_thread_data(instance).path;
    asset_uri_to_path(instance, asset.uri.view(), &src_path);

    g_pipeline->asset_op_deps.schedule_write(asset.uri, create_null_job());

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
    g_assetdb.delete_asset(instance->db, txn, guid);
    g_assetdb.commit(instance->db, &txn);

    // Delete .meta file
    src_path.append_extension(g_metadata_ext);

    // src_path is now meta_path
    if (src_path.exists() && !fs::remove(src_path))
    {
        log_error("Failed to delete asset: invalid %s file location %s", g_metadata_ext, src_path.c_str());
    }
}

void delete_asset_at_path(AssetPipeline* instance, const StringView& uri, const DeleteAssetKind kind)
{
    auto& asset = get_temp_asset(instance);
    auto txn = g_assetdb.read(instance->db);

    if (!g_assetdb.get_asset_from_path(instance->db, txn, uri, &asset))
    {
        return;
    }

    g_assetdb.commit(instance->db, &txn);

    delete_asset(instance, asset.metadata.guid, kind);
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
    info.instance = compiler;
    new (&info.id) AssetCompilerId(hash);

    const auto compiler_id = g_pipeline->compilers.size() - 1;

    // Validate that no compilers have been registered with the supported extensions
    const int filetype_count = compiler->supported_file_types(nullptr);
    if (BEE_FAIL_F(filetype_count > 0, "Asset compiler must specify at least one supported file type"))
    {
        return;
    }

    auto* supported_filetypes = BEE_ALLOCA_ARRAY(const char*, filetype_count);
    compiler->supported_file_types(supported_filetypes);

    for (int i = 0; i < filetype_count; ++i)
    {
        const auto* ext = supported_filetypes[i];
        const auto ext_hash = get_extension_hash(ext);

        if (find_index_if(info.extensions, [&](const u32 hash)
        {
            return hash == ext_hash;
        }) >= 0)
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

    if (info.instance->init != nullptr)
    {
        info.instance->init(info.instance->data, get_job_worker_count());
    }

    if (info.instance->get_order != nullptr)
    {
        info.order = info.instance->get_order();
    }

    // TODO(Jacob): iterate watched directories and import all assets with a filetype in supported_filetypes that
    //  haven't already been imported OR have a newer timestamp than what's stored in the AssetDB
}

void unregister_compiler(AssetCompiler* compiler)
{
    const auto* name = compiler->get_name();
    const auto hash = get_hash(name);
    auto id = find_compiler(hash);

    if (id < 0)
    {
        return;
    }

    for (const auto ext_hash : g_pipeline->compilers[id].extensions)
    {
        auto* extension_mapping = g_pipeline->filetype_map.find(ext_hash);
        if (extension_mapping != nullptr)
        {
            const auto compiler_mapping_idx = find_index_if(extension_mapping->value.compiler_ids, [&](const i32 stored)
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

    if (g_pipeline->compilers[id].instance->destroy != nullptr)
    {
        g_pipeline->compilers[id].instance->destroy(g_pipeline->compilers[id].instance->data);
    }

    g_pipeline->compilers.erase(id);
}

void add_asset_directory(AssetPipeline* instance, const Path& path)
{
    if (!path.exists())
    {
        log_error("The asset root path does not exist");
        return;
    }

    instance->asset_watcher.add_directory(path);
}

void remove_asset_directory(AssetPipeline* instance, const Path& path)
{
    instance->asset_watcher.remove_directory(path);
}

Span<const Path> asset_directories(AssetPipeline* instance)
{
    return instance->asset_watcher.watched_directories();
}

void refresh_path(AssetImportBatch* batch, const Path& path)
{
    Path src_path(path.view(), temp_allocator());
    Path metadata_path(path.view(), temp_allocator());

    const auto is_asset_file = path.extension() == g_metadata_ext;

    // Get the source path from the .meta file at path instead
    if (is_asset_file)
    {
        src_path.set_extension("");
    }
    else
    {
        metadata_path.append_extension(g_metadata_ext);
    }

    auto* pipeline = batch->pipeline;
    auto& uri = get_thread_data(pipeline).uri;
    asset_path_to_uri(pipeline, src_path, &uri);

    g_pipeline->asset_op_deps.schedule_read(uri, create_null_job());

    // Check if we've already imported the source file
    auto txn = g_assetdb.read(pipeline->db);
    auto& asset = get_temp_asset(pipeline);
    const auto is_reimport = g_assetdb.get_asset_from_path(pipeline->db, txn, uri.view(), &asset);
    g_assetdb.commit(pipeline->db, &txn);

    // if the source path is missing, delete the asset from the DB
    if (!src_path.exists())
    {
        delete_asset_at_path(pipeline, uri.view(), DeleteAssetKind::asset_only);
        return;
    }

    // Check timestamps as a quick first change test
    const auto metadata_exists = metadata_path.exists();
    const auto src_timestamp = fs::last_modified(src_path);
    const auto metadata_timestamp = metadata_exists ? fs::last_modified(metadata_path) : 0;

    if (is_reimport)
    {
        if (!metadata_exists)
        {
            delete_asset_at_path(pipeline, uri.view(), DeleteAssetKind::asset_only);
            return;
        }

        if (src_timestamp == asset.src_timestamp && metadata_timestamp == asset.metadata_timestamp)
        {
            return;
        }
    }

    BEE_ASSERT(src_path.extension() != g_metadata_ext);

    auto& metadata = asset.metadata;

    if (metadata_exists)
    {
        read_metadata(metadata_path, &metadata);
    }

    if (is_reimport)
    {
        // Check content hash as a full change test
        const auto source_hash = get_source_hash(src_path, metadata.settings);

        // same content hashes - no change
        if (source_hash == asset.source_hash)
        {
            return;
        }

        asset.source_hash = source_hash;
    }
    else
    {
        asset.source_hash = get_source_hash(src_path, metadata.settings);
    }

    // disk_asset may have been moved into stored_asset here - DO NOT USE IT - only use stored_asset

    if (fs::is_file(src_path))
    {
        batch->add(src_path, metadata.compiler);
    }
    else
    {
        if (!is_reimport)
        {
            metadata.guid = generate_guid();
        }

        metadata.is_directory = true;

        asset.src_timestamp = src_timestamp;
        asset.metadata_timestamp = write_metadata(metadata_path, &metadata);
        asset.uri.append(uri.view());

        txn = g_assetdb.write(pipeline->db);
        g_assetdb.put_asset(pipeline->db, txn, metadata.guid, &asset);
        g_assetdb.commit(pipeline->db, &txn);

        log_debug("%s directory %s", is_reimport ? "Reimported" : "Imported", uri.c_str());

        if (!is_asset_file)
        {
            for (const auto& child : fs::read_dir(src_path))
            {
                refresh_path(batch, child);
            }
        }
    }
}

void refresh(AssetPipeline* instance)
{
    auto events = instance->asset_watcher.pop_events();

    if (!events.empty())
    {
        auto* batch = create_import_batch(instance);

        for (auto& event : events)
        {
            refresh_path(batch, event.file);
        }

        schedule_import_batch(batch);
    }

    g_pipeline->asset_op_deps.trim();
}


/*
 * Locator
 */

static AssetLocator g_assetdb_locator{};

/*
 * Import roots as they're registered
 */
void load_manifests_at_path(AssetPipeline* instance, const Path& path)
{
    g_pipeline->asset_op_deps.wait_all();

    // iterate through the plugins source directory for any .root files and add the roots if they're there
    JSONSerializer serializer(temp_allocator());
    ManifestFile manifest_file(temp_allocator());
    int files_added = 0;

    for (const auto& file : fs::read_dir(path))
    {
        if (fs::is_dir(file))
        {
            continue;
        }

        if (file.extension() != ".manifest")
        {
            continue;
        }

        auto contents = fs::read(file, temp_allocator());
        serializer.reset(contents.data(), rapidjson::kParseInsituFlag);
        serialize(
            SerializerMode::reading,
            SerializerSourceFlags::dont_serialize_flags | SerializerSourceFlags::unversioned,
            &serializer,
            &manifest_file,
            temp_allocator()
        );

        auto* manifest = g_asset_registry->get_manifest(manifest_file.name.view());
        if (manifest == nullptr)
        {
            manifest = g_asset_registry->add_manifest(manifest_file.name.view());
        }

        auto txn = g_assetdb.read(instance->db);

        for (const auto& entry : manifest_file.assets)
        {
            auto& asset = get_temp_asset(instance);
            if (!g_assetdb.get_asset_from_path(instance->db, txn, entry.value.view(), &asset))
            {
                log_error("No imported asset found at path %s", entry.value.c_str());
                continue;
            }

            const auto hash = detail::runtime_fnv1a(entry.key.data(), entry.key.size());
            if (manifest->add(hash, asset.metadata.guid))
            {
                ++files_added;
            }
        }
    }

    if (files_added > 0)
    {
        serialize_manifests(SerializerMode::writing, instance);
    }
}

void plugin_observer(const PluginEventType event, const bee::PluginDescriptor& plugin, const StringView& module_name, void* module, void* user_data)
{
    auto* instance = static_cast<AssetPipeline*>(user_data);
    Path path(temp_allocator());
    plugin.get_full_path(&path);
    load_manifests_at_path(instance, path);
}

const char* get_runtime_locator_name()
{
    return "Bee.AssetPipeline Locator";
}

bool runtime_locate_asset(AssetLocatorInstance* instance, const GUID& guid, AssetLocation* location)
{
    auto txn = g_assetdb.read(instance->pipeline->db);
    const auto artifact_count = g_assetdb.get_artifacts_from_guid(instance->pipeline->db, txn, guid, nullptr);
    if (artifact_count <= 0)
    {
        log_error("No artifacts");
        return false;
    }

    auto& asset = get_temp_asset(instance->pipeline);
    if (!g_assetdb.get_asset(instance->pipeline->db, txn, guid, &asset))
    {
        log_error("No asset");
        return false;
    }

    BEE_ASSERT(artifact_count < AssetLocation::max_streams);

    auto* artifacts = BEE_ALLOCA_ARRAY(AssetArtifact, artifact_count);
    location->type = get_type(asset.main_artifact.type_hash);
    location->stream_count = g_assetdb.get_artifacts_from_guid(instance->pipeline->db, txn, guid, artifacts);

    BEE_ASSERT(location->stream_count == artifact_count);

    for (int i = 0; i < artifact_count; ++i)
    {
        location->streams[i].asset_type = get_type(artifacts[i].type_hash);
        location->streams[i].stream_type = AssetStreamType::file;
        location->streams[i].offset = 0;
        g_assetdb.get_artifact_path(instance->pipeline->db, artifacts[i].content_hash, &location->streams[i].path);
    }

    return true;
}


} // namespace bee


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    bee::g_plugin_registry = registry;
    bee::g_asset_registry = registry->get_module<bee::AssetRegistryModule>(BEE_ASSET_REGISTRY_MODULE_NAME);

    load_assetdb_module(registry, state);

    bee::g_pipeline = registry->get_or_create_persistent<bee::GlobalAssetPipeline>("GlobalAssetPipelineData");

    bee::g_module.init = bee::init;
    bee::g_module.destroy = bee::destroy;
    bee::g_module.set_platform = bee::set_platform;
    bee::g_module.import_asset = bee::import_asset;
    bee::g_module.delete_asset = bee::delete_asset;
    bee::g_module.register_compiler = bee::register_compiler;
    bee::g_module.unregister_compiler = bee::unregister_compiler;
    bee::g_module.get_compilers_for_filetype = bee::get_compilers_for_filetype;
    bee::g_module.add_asset_directory = bee::add_asset_directory;
    bee::g_module.remove_asset_directory = bee::remove_asset_directory;
    bee::g_module.asset_directories = bee::asset_directories;
    bee::g_module.refresh = bee::refresh;

    for (auto& pipeline : bee::g_pipeline->all_pipelines)
    {
        pipeline->locator.get_name = bee::get_runtime_locator_name;
        pipeline->locator.locate = bee::runtime_locate_asset;
    }

    registry->toggle_module(state, BEE_ASSET_PIPELINE_MODULE_NAME, &bee::g_module);
}