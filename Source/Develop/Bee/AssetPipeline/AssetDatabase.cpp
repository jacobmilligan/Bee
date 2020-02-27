/*
 *  AssetDatabase.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#define BEE_RAPIDJSON_ERROR_H
#include "Bee/AssetPipeline/AssetDatabase.hpp"
#include "Bee/AssetPipeline/AssetCompiler.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Thread.hpp"
#include "Bee/Core/Serialization/JSONSerializer.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Memory/PoolAllocator.hpp"
#include "Bee/Core/Containers/ResourcePool.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"
#include "Bee/Core/Memory/LinearAllocator.hpp"

#include <lmdb.h>

namespace bee {


/*
 **************************
 *
 * AssetDB data
 *
 **************************
 */
static constexpr auto g_artifacts_dirname = "Artifacts";
static constexpr auto g_db_name = "AssetDB";
static constexpr auto g_assets_dbi_name = "Assets";
static constexpr auto g_namemap_dbi_name = "NameMap";
static constexpr auto g_invalid_dbi = limits::max<u32>();

static Path                     g_path; // NOLINT
static Path                     g_artifacts_path; // NOLINT
static MDB_env*                 g_env { nullptr };
static MDB_dbi                  g_assets_dbi { 0 };
static MDB_dbi                  g_namemap_dbi { 0 };
static MDB_dbi                  g_artifacts_dbi { 0 };
static JobGroup                 g_compile_jobs;
static AtomicStack              g_asset_meta_pool;
static AssetCompilerPipeline*   g_compiler_pipeline { nullptr };

AtomicNodePtr<AssetMeta> get_or_create_asset_meta()
{
    auto node = g_asset_meta_pool.pop();
    if (node != nullptr)
    {
        AtomicNodePtr<AssetMeta> ptr;
        ptr.node = node;
        ptr.data = static_cast<AssetMeta*>(node->data[0]);
        return ptr;
    }
    return make_atomic_node<AssetMeta>(system_allocator());
}

void release_asset_meta(AtomicNodePtr<AssetMeta>* ptr)
{
    g_asset_meta_pool.push(ptr->node);
    ptr->node = nullptr;
    ptr->data = nullptr;
}

struct SerializedOptionsSizeCache
{
    ReaderWriterMutex               mutex;
    DynamicArray<u32>               type_hashes;
    DynamicArray<i32>               sizes;
    DynamicArray<LinearAllocator>   per_thread_temp_allocators;

    ~SerializedOptionsSizeCache()
    {
        clear();
    }

    i32 find_or_insert(const Type* type)
    {
        {
            scoped_rw_read_lock_t lock(mutex);
            const auto index = container_index_of(type_hashes, [&](const u32 hash)
            {
                return hash == type->hash;
            });

            if (index >= 0)
            {
                return sizes[index];
            }
        }

        // Serialize an instance of the type so we can calculate its serialized size
        auto instance = type->create_instance(system_allocator());

        DynamicArray<u8> serialization_buffer;
        BinarySerializer serializer(&serialization_buffer);
        serialize(SerializerMode::writing, &serializer, &instance);

        // Lock and add info to the cache
        scoped_rw_write_lock_t lock(mutex);
        type_hashes.push_back(type->hash);
        sizes.push_back(serialization_buffer.size());
        return sizes.back();
    }

    void clear()
    {
        scoped_rw_write_lock_t lock(mutex);
        type_hashes.clear();
        sizes.clear();
    }
};

static SerializedOptionsSizeCache g_options_size_cache;


/*
 *********************
 *
 * LMDB utilities
 *
 *********************
 */
#define BEE_LMDB_FAIL(lmdb_result) BEE_FAIL_F(lmdb_result == 0, "LMDB error (%d): %s", lmdb_result, mdb_strerror(lmdb_result))

#define BEE_LMDB_ASSERT(lmdb_result) BEE_ASSERT_F(lmdb_result == 0, "LMDB error (%d): %s", lmdb_result, mdb_strerror(lmdb_result))

void lmdb_assert_callback(MDB_env* env, const char* msg)
{
    BEE_ERROR("LMDB", "%s", msg);
#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    BEE_DEBUG_BREAK();
    abort();
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1
}

bool mdb_is_valid()
{
    return g_env != nullptr && g_assets_dbi != g_invalid_dbi && g_namemap_dbi != g_invalid_dbi;
}

MDB_val mdb_make_key(const char* name, const i32 name_length)
{
    MDB_val val{};
    val.mv_size = sizeof(char) * name_length;
    val.mv_data = const_cast<char*>(name);
    return val;
}

MDB_val mdb_make_key(const u128& hash)
{
    MDB_val val{};
    val.mv_size = sizeof(u128);
    val.mv_data = const_cast<u8*>(reinterpret_cast<const u8*>(&hash));
    return val;
}

// Temporary buffer for storing records
static thread_local char value_buffer[4096];

MDB_val mdb_make_key(const GUID& guid)
{
    MDB_val val{};
    val.mv_size = static_array_length(guid.data);
    val.mv_data = const_cast<u8*>(guid.data);
    return val;
}

void mdb_get_value(const MDB_val& val, GUID* guid)
{
    BEE_ASSERT(val.mv_size >= static_array_length(guid->data));

    memcpy(guid, val.mv_data, static_array_length(guid->data));
}

MDB_val mdb_make_value(const Span<const AssetCompilerContext::Artifact>& artifacts)
{
    io::MemoryStream stream(value_buffer, static_array_length(value_buffer), 0);

    int artifacts_size = artifacts.size();
    stream.write(&artifacts_size, sizeof(i32));
    for (const AssetCompilerContext::Artifact& artifact : artifacts)
    {
        stream.write(&artifact.hash, sizeof(u128));
    }

    MDB_val val{};
    val.mv_data = value_buffer;
    val.mv_size = static_cast<size_t>(stream.size());
    return val;
}

void mdb_get_value(const MDB_val& val, DynamicArray<u128>* artifact_hashes)
{
    io::MemoryStream stream(const_cast<const void*>(val.mv_data), val.mv_size);

    int size = 0;
    stream.read(&size, sizeof(i32));
    for (int i = 0; i < size; ++i)
    {
        artifact_hashes->push_back_no_construct();
        stream.read(&artifact_hashes->back(), sizeof(u128));
    }
}

struct Scoped_MDB_txn
{
    MDB_txn* ptr {nullptr };

    void commit()
    {
        mdb_txn_commit(ptr);
        ptr = nullptr;
    }

    ~Scoped_MDB_txn()
    {
        if (ptr != nullptr)
        {
            mdb_txn_abort(ptr);
        }
        ptr = nullptr;
    }
};

Scoped_MDB_txn mdb_begin_read_write()
{
    BEE_ASSERT(mdb_is_valid());

    MDB_txn* txn = nullptr;

    const auto result = mdb_txn_begin(g_env, nullptr, 0, &txn);
    BEE_LMDB_ASSERT(result);

    return  { txn };
}

Scoped_MDB_txn mdb_begin_read_only()
{
    BEE_ASSERT(mdb_is_valid());

    MDB_txn* txn = nullptr;

    const auto result = mdb_txn_begin(g_env, nullptr, MDB_RDONLY, &txn);
    BEE_LMDB_ASSERT(result);

    return { txn };
}

bool mdb_put_asset(MDB_txn* txn, const AssetMeta& meta, TypeInstance* options)
{
    // Update the data in the asset dbi
    auto key = mdb_make_key(meta.guid);

    MDB_val val{};
    val.mv_size = sizeof(AssetMeta) + g_options_size_cache.find_or_insert(options->type());

    if (BEE_LMDB_FAIL(mdb_put(txn, g_assets_dbi, &key, &val, MDB_RESERVE)))
    {
        return false;
    }

    io::MemoryStream stream(val.mv_data, val.mv_size, 0);
    stream.write(&meta, sizeof(AssetMeta));
    StreamSerializer serializer(&stream);
    serialize(SerializerMode::writing, &serializer, options);

    // If the meta has a valid name then update that too
    if (!meta.name.empty())
    {
        key = mdb_make_key(meta.name.c_str(), meta.name.size());
        val = mdb_make_key(meta.guid);

        if (BEE_LMDB_FAIL(mdb_put(txn, g_namemap_dbi, &key, &val, 0)))
        {
            return false;
        }
    }



    return true;
}

bool mdb_get_asset(MDB_txn* txn, const GUID& guid, AssetMeta** meta, TypeInstance* instance)
{
    auto key = mdb_make_key(guid);
    MDB_val val{};

    const auto result = mdb_get(txn, g_assets_dbi, &key, &val);
    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    if (meta != nullptr)
    {
        *meta = static_cast<AssetMeta*>(val.mv_data);
    }

    if (instance != nullptr)
    {
        io::MemoryStream stream(val.mv_data, val.mv_size - sizeof(AssetMeta));
        stream.seek(sizeof(AssetMeta), io::SeekOrigin::begin);
        StreamSerializer serializer(&stream);
        serialize(SerializerMode::reading, &serializer, instance);
    }

    return true;
}


bool mdb_put_artifacts(MDB_txn* txn, const u128& content_hash, const Span<const AssetCompilerContext::Artifact>& artifacts)
{
    auto key = mdb_make_key(content_hash);
    auto val = mdb_make_value(artifacts);

    if (BEE_LMDB_FAIL(mdb_put(txn, g_artifacts_dbi, &key, &val, 0)))
    {
        return false;
    }

    return true;
}

bool mdb_get_artifacts(MDB_txn* txn, const u128& content_hash, DynamicArray<u128>* artifact_hashes)
{
    auto key = mdb_make_key(content_hash);
    MDB_val val{};

    const auto result = mdb_get(txn, g_artifacts_dbi, &key, &val);
    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    mdb_get_value(val, artifact_hashes);
    return true;
}

bool mdb_put_name(MDB_txn* txn, const StringView& name, const GUID& guid)
{
    auto key = mdb_make_key(name.c_str(), name.size());
    auto val = mdb_make_key(guid);

    if (BEE_LMDB_FAIL(mdb_put(txn, g_namemap_dbi, &key, &val, 0)))
    {
        return false;
    }

    return true;
}

bool mdb_get_name(MDB_txn* txn, const StringView& name, GUID* guid)
{
    auto key = mdb_make_key(name.c_str(), name.size());
    MDB_val val{};

    const auto result = mdb_get(txn, g_namemap_dbi, &key, &val);
    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    mdb_get_value(val, guid);
    return true;
}


/*
 **************************
 *
 * AssetDB implementation
 *
 **************************
 */
void assetdb_open(const Path& root, AssetCompilerPipeline* compiler_pipeline)
{
    if (BEE_FAIL_F(current_thread::is_main(), "AssetDB can only be opened from the main thread"))
    {
        return;
    }

    if (BEE_FAIL_F(g_env == nullptr, "AssetDB is already opened at path: %s", g_path.c_str()))
    {
        return;
    }

    if (BEE_FAIL_F(root.exists(), "Cannot open AssetDB: root path \"%s\" does not exist", root.c_str()))
    {
        return;
    }

    g_path = root.join(g_db_name);
    g_artifacts_path = root.join(g_artifacts_dirname);

    if (!g_artifacts_path.exists())
    {
        const auto mkdir_success = fs::mkdir(g_artifacts_path);
        BEE_ASSERT(mkdir_success);
    }

    if (BEE_LMDB_FAIL(mdb_env_create(&g_env)))
    {
        assetdb_close();
        return;
    }

    // Setup assertions and max DBI's for environment - MUST BE CONFIGURED PRIOR TO `mdb_env_open`
    const auto result = mdb_env_set_assert(g_env, &lmdb_assert_callback);
    BEE_LMDB_ASSERT(result);

    if (BEE_LMDB_FAIL(mdb_env_set_maxdbs(g_env, 3)))
    {
        assetdb_close();
        return;
    }

    /*
     * - Default flags
     * - unix permissions (ignored on windows): -rw-rw-r--
     * - NOSUBDIR - custom database filename
     */
    if (BEE_LMDB_FAIL(mdb_env_open(g_env, g_path.c_str(), MDB_NOSUBDIR, 0664)))
    {
        assetdb_close();
        return;
    }

    MDB_txn* txn = nullptr;
    if (BEE_LMDB_FAIL(mdb_txn_begin(g_env, nullptr, 0, &txn)))
    {
        return;
    }

    // Open handles to both databases - name map and asset storage
    const auto asset_dbi_result = mdb_dbi_open(txn, g_assets_dbi_name, MDB_CREATE, &g_assets_dbi);
    const auto name_dbi_result = mdb_dbi_open(txn, g_namemap_dbi_name, MDB_CREATE, &g_namemap_dbi);
    const auto artifacts_dbi_result = mdb_dbi_open(txn, g_artifacts_dirname, MDB_CREATE, &g_artifacts_dbi);

    if (BEE_LMDB_FAIL(asset_dbi_result) || BEE_LMDB_FAIL(name_dbi_result) || BEE_LMDB_FAIL(artifacts_dbi_result))
    {
        mdb_txn_abort(txn);
        assetdb_close();
        return;
    }

    BEE_LMDB_ASSERT(mdb_txn_commit(txn));

    g_compiler_pipeline = compiler_pipeline;
}

void assetdb_close()
{
    if (BEE_FAIL_F(current_thread::is_main(), "AssetDB can only be opened from the main thread"))
    {
        return;
    }

    job_wait(&g_compile_jobs);

    if (g_env == nullptr)
    {
        return;
    }

    if (g_assets_dbi != g_invalid_dbi)
    {
        mdb_dbi_close(g_env, g_assets_dbi);
    }

    if (g_namemap_dbi != g_invalid_dbi)
    {
        mdb_dbi_close(g_env, g_namemap_dbi);
    }

    if (g_artifacts_dbi != g_invalid_dbi)
    {
        mdb_dbi_close(g_env, g_artifacts_dbi);
    }

    mdb_env_close(g_env);
    g_env = nullptr;
}

void write_asset_file(AssetFile* file, Allocator* tmp_json_allocator)
{
    JSONSerializer serializer(tmp_json_allocator);
    serialize(SerializerMode::writing, &serializer, file);
    fs::write(file->meta.location.view(), serializer.c_str());
}

void read_asset_file(AssetFile* file, Allocator* tmp)
{
    const auto location = Path(file->meta.location.view(), tmp);
    if (BEE_FAIL_F(location.exists(), "AssetDB: no .asset file located at \"%s\"", location.c_str()))
    {
        return;
    }

    auto src = fs::read(location, tmp);
    JSONSerializer serializer(src.data(), rapidjson::ParseFlag::kParseInsituFlag, tmp);
    serialize(SerializerMode::reading, &serializer, file);
}

void import(const AssetMeta& meta)
{

    AssetFile asset_file;
    asset_file.meta = meta;
    asset_file.meta.guid = generate_guid();

    auto options_type = g_compiler_pipeline->get_options_type(asset_file.meta.compiler);

    if (!options_type->is(TypeKind::unknown))
    {
        asset_file.options = std::move(options_type->create_instance(temp_allocator()));
    }

    asset_file.meta.content_hash = assetdb_get_content_hash(default_asset_platform, asset_file);

    auto txn = mdb_begin_read_write();

    if (mdb_get_artifacts(txn.ptr, asset_file.meta.content_hash, nullptr))
    {
        return;
    }

    write_asset_file(&asset_file, temp_allocator());

    if (!mdb_put_asset(txn.ptr, asset_file.meta, asset_file.options.is_valid() ? &asset_file.options : nullptr))
    {
        return;
    }

    auto compiler = g_compiler_pipeline->get_compiler(asset_file.meta.compiler);
    AssetCompilerContext ctx(default_asset_platform, meta.location.view(), asset_file.options, temp_allocator());
    const auto result = compiler->compile(get_local_job_worker_id(), &ctx);
    ctx.calculate_hashes();

    if (result != AssetCompilerStatus::success)
    {
        log_error("Failed to compile asset: %s", asset_compiler_status_to_string(result));
        return;
    }

    mdb_put_artifacts(txn.ptr, asset_file.meta.content_hash, ctx.artifacts().const_span());

    Path artifact_path(temp_allocator());
    String artifact_hash(temp_allocator());

    for (const AssetCompilerContext::Artifact& artifact : ctx.artifacts())
    {
        artifact_hash.clear();
        io::write_fmt(&artifact_hash, "%" BEE_PRIxu128, BEE_FMT_u128(artifact.hash));

        artifact_path.clear();
        artifact_path.append(g_artifacts_path).append(StringView(artifact_hash.c_str(), 2));
        
        // Ensure the cache folder exists, i.e. 52df92038 -> 52/
        if (!artifact_path.exists())
        {
            fs::mkdir(artifact_path);
        }

        artifact_path.append(artifact_hash.view());

        fs::write(artifact_path, artifact.buffer.const_span());
    }

    txn.commit();
}

void assetdb_import(const StringView& name, const Path& source_path, const Path& target_folder, JobGroup* wait_group)
{
    // ensure the relative_source file exists
    if (BEE_FAIL_F(source_path.exists(), "Failed to import asset from relative_source at \"%s\": file does not exist", source_path.c_str()))
    {
        return;
    }

    // ensure we're targeting a valid project folder
    if (BEE_FAIL_F(fs::is_dir(target_folder), "Failed to import asset: \"%s\" is not a valid target folder", target_folder.c_str()))
    {
        return;
    }

    // ensure there's not a .asset file already imported to that location
    auto dst_path = target_folder.join(source_path.filename()).set_extension("asset");
    if (BEE_FAIL_F(!dst_path.exists(), "Failed to import asset: an asset already exists at path \"%s\"", dst_path.c_str()))
    {
        return;
    }

    const auto compilers = g_compiler_pipeline->get_compiler_hashes(source_path.view());
    if (BEE_FAIL_F(!compilers.empty(), "Failed to import asset: no asset compilers were registered for file type with extension \"%" BEE_PRIsv "\"", BEE_FMT_SV(source_path.extension())))
    {
        return;
    }

    // We only handle GUID generation in the import function in case the job gets cancelled etc.
    const auto relative_source = source_path.relative_to(target_folder, temp_allocator());

    if (wait_group != nullptr)
    {
        auto meta = get_or_create_asset_meta();
        meta.data->source = relative_source.view();
        meta.data->name = name;
        meta.data->location = dst_path.view();
        meta.data->compiler = compilers[0];

        auto job = create_job([=]() mutable
        {
            import(*meta.data);
            release_asset_meta(&meta);
        });

        job_schedule(wait_group, job);
    }
    else
    {
        AssetMeta meta{};
        meta.source = relative_source.view();
        meta.name = name;
        meta.location = dst_path.view();
        meta.compiler = compilers[0];
        import(meta);
    }
}

void assetdb_import(const Path& source_path, const Path& target_folder, JobGroup* wait_group)
{
    assetdb_import({}, source_path, target_folder, wait_group);
}

void asset_db_save()
{}

bool assetdb_get_guid(const StringView& name, GUID* dst_guid)
{
    auto txn = mdb_begin_read_only();
    if (BEE_FAIL_F(mdb_get_name(txn.ptr, name, dst_guid), "AssetDB: could not get GUID for \"%" BEE_PRIsv "\": no asset with that name exists", BEE_FMT_SV(name)))
    {
        return false;
    }

    return true;
}


AssetDBTxn assetdb_transaction(const AssetDBTxn::Kind kind, const GUID& guid, const Type* type)
{
    BEE_ASSERT(mdb_is_valid());

    MDB_txn* txn = nullptr;

    const auto flags = kind == AssetDBTxn::Kind::read_only ? MDB_RDONLY : 0;
    const auto result = mdb_txn_begin(g_env, nullptr, flags, &txn);

    BEE_LMDB_ASSERT(result);

    AssetMeta* meta = nullptr;
    TypeInstance options;
    if (!mdb_get_asset(txn, guid, &meta, &options))
    {
        return AssetDBTxn{};
    }

    return AssetDBTxn(kind, txn, meta, std::move(options));
}

void AssetDBTxn::move_construct(bee::AssetDBTxn& other) noexcept
{
    destroy();
    kind_ = other.kind_;
    meta_ = other.meta_;
    txn_ = other.txn_;
    options_ = std::move(other.options_);

    other.kind_ = Kind::invalid;
    other.meta_ = nullptr;
    other.txn_ = nullptr;
}

void AssetDBTxn::destroy()
{
    switch(kind_)
    {
        case Kind::read_write:
        {
            abort();
            break;
        }
        case Kind::read_only:
        {
            commit();
        }
        default: break;
    }

    kind_ = Kind::invalid;
    meta_ = nullptr;
    txn_ = nullptr;
    destruct(&options_);
}

AssetDBTxn::~AssetDBTxn()
{
    destroy();
}

void AssetDBTxn::abort()
{
    if (kind_ == Kind::invalid || txn_ == nullptr)
    {
        return;
    }

    mdb_txn_abort(txn_);
    kind_ = Kind::invalid;
    destroy();
}

void AssetDBTxn::commit()
{
    if (kind_ == Kind::invalid || txn_ == nullptr)
    {
        return;
    }

    if (kind_ == Kind::read_write)
    {
        mdb_put_asset(txn_, *meta_, &options_);
    }

    // copy the meta file before commiting the transaction otherwise we might lose it
    AssetFile file;
    file.meta = *meta_;

    mdb_txn_commit(txn_);

    file.options = std::move(options_); // move the options - we don't need them anymore

    // Persist the .asset file to disk
    write_asset_file(&file, temp_allocator());

    kind_ = Kind::invalid;
    destroy();
}

u128 assetdb_get_content_hash(const AssetPlatform platform, const AssetFile& asset)
{
    static thread_local u8 buffer[4096];

    bee::HashState128 state;
    state.add(asset.meta.guid);
    state.add(asset.meta.compiler);
    if (asset.options.is_valid())
    {
        state.add(asset.options.data(), asset.options.type()->size);
    }
    state.add(platform);

    // TODO(Jacob): replace with memory-mapped files
    const auto source_path = Path(asset.meta.location.view()).parent().join(asset.meta.source.view());
    io::FileStream file(source_path, "rb");
    while (true)
    {
        const auto read_size = file.read(buffer, static_array_length(buffer));
        if (read_size <= 0)
        {
            break;
        }
        state.add(buffer, read_size);
    }

    return state.end();
}

} // namespace bee