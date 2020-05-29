/*
 *  AssetDatabase.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Plugins/AssetPipeline/AssetPipeline.hpp"
#include "Bee/Plugins/AssetRegistry/AssetRegistry.hpp"
#include "Bee/Core/Serialization/BinarySerializer.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

#include <lmdb.h>

namespace bee {


static constexpr auto g_artifacts_dirname = "Artifacts";
static constexpr auto g_invalid_dbi = limits::max<u32>();

AssetDatabaseModule g_assetdb{};

/*
 *********************
 *
 * LMDB API
 *
 *********************
 */
#define BEE_LMDB_FAIL(lmdb_result) BEE_FAIL_F(lmdb_result == 0, "LMDB error (%d): %s", lmdb_result, mdb_strerror(lmdb_result))

#define BEE_LMDB_ASSERT(lmdb_result) BEE_ASSERT_F(lmdb_result == 0, "LMDB error (%d): %s", lmdb_result, mdb_strerror(lmdb_result))

void lmdb_assert_callback(MDB_env* env, const char* msg)
{
    log_error("LMDB: %s", msg);
#if BEE_CONFIG_ENABLE_ASSERTIONS == 1
    BEE_DEBUG_BREAK();
    abort();
#endif // BEE_CONFIG_ENABLE_ASSERTIONS == 1
}

MDB_val make_key(const StringView& name)
{
    MDB_val val{};
    val.mv_size = sizeof(char) * name.size();
    val.mv_data = const_cast<char*>(name.data());
    return val;
}

MDB_val make_key(const GUID& guid)
{
    MDB_val val{};
    val.mv_size = static_array_length(guid.data);
    val.mv_data = const_cast<u8*>(guid.data);
    return val;
}

MDB_val make_key(const AssetArtifact& artifact)
{
    MDB_val val{};
    val.mv_size = sizeof(AssetArtifact);
    val.mv_data = const_cast<void*>(reinterpret_cast<const void*>(&artifact));
    return val;
}

MDB_val make_key(const u32 hash)
{
    MDB_val val{};
    val.mv_size = sizeof(u32);
    val.mv_data = const_cast<u32*>(&hash);
    return val;
}

bool txn_get(const AssetDbTxn& txn, const unsigned int dbi, MDB_val* key, MDB_val* val)
{
    const auto result = mdb_get(txn.handle, dbi, key, val);
    return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
}

bool txn_del(const AssetDbTxn& txn, const unsigned int dbi, MDB_val* key, MDB_val* val)
{
    const auto result = mdb_del(txn.handle, dbi, key, val);
    return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
}

bool txn_put(const AssetDbTxn& txn, const unsigned int dbi, MDB_val* key, MDB_val* val, const unsigned int flags)
{
    const auto result = mdb_put(txn.handle, dbi, key, val, flags);
    return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
}

struct LMDBCursor
{
    MDB_cursor* cursor { nullptr };

    LMDBCursor(const AssetDbTxn& txn, const MDB_dbi dbi)
    {
        if (BEE_LMDB_FAIL(mdb_cursor_open(txn.handle, dbi, &cursor)))
        {
            cursor = nullptr;
        }
    }

    ~LMDBCursor()
    {
        if (cursor != nullptr)
        {
            close();
        }
    }

    explicit operator bool() const
    {
        return cursor != nullptr;
    }

    bool get(MDB_val* key, MDB_val* val, const MDB_cursor_op op) const
    {
        const auto result = mdb_cursor_get(cursor, key, val, op);
        return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
    }

    bool del(const unsigned int flags = 0) const
    {
        const auto result = mdb_cursor_del(cursor, flags);
        return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
    }

    bool put(MDB_val* key, MDB_val* val, const unsigned int flags = 0) const
    {
        const auto result = mdb_cursor_put(cursor, key, val, flags);
        return !BEE_LMDB_FAIL(result);
    }

    i32 count() const
    {
        mdb_size_t result = 0;
        if (BEE_LMDB_FAIL(mdb_cursor_count(cursor, &result)))
        {
            return -1;
        }
        return sign_cast<i32>(result);
    }

    void close()
    {
        mdb_cursor_close(cursor);
        cursor = nullptr;
    }
};

int lmdb_compare_guid(const MDB_val* a, const MDB_val* b)
{
    const auto& lhs = *static_cast<const GUID*>(a->mv_data);
    const auto& rhs = *static_cast<const GUID*>(b->mv_data);

    if (lhs < rhs)
    {
        return -1;
    }

    if (lhs > rhs)
    {
        return 1;
    }

    return 0;
}

int lmdb_compare_artifact(const MDB_val* a, const MDB_val* b)
{
    const auto& lhs = *static_cast<const AssetArtifact*>(a->mv_data);
    const auto& rhs = *static_cast<const AssetArtifact*>(b->mv_data);

    if (lhs.content_hash < rhs.content_hash)
    {
        return -1;
    }

    if (lhs.content_hash > rhs.content_hash)
    {
        return 1;
    }

    return 0;
}

/*
 ********************************
 *
 * Database - implementation
 *
 ********************************
 */
enum class DbMapId
{
    guid_to_asset,
    guid_to_dependencies,
    guid_to_artifact,
    artifact_to_guid,
    path_to_guid,
    type_to_guid,
    count
};

struct DbMapInfo
{
    const char*     name { nullptr };
    unsigned int    flags { 0 };
    MDB_cmp_func*   dupsort_func { nullptr };
};

BEE_TRANSLATION_TABLE(db_mapping_info, DbMapId, DbMapInfo, DbMapId::count,
    { "GUIDToAsset", MDB_CREATE },                                          // guid_to_asset
    { "GUIDToDependencies", MDB_CREATE | MDB_DUPSORT, lmdb_compare_guid },  // guid_to_dependencies
    { "GUIDToArtifact", MDB_CREATE | MDB_DUPSORT, lmdb_compare_artifact },  // guid_to_artifact
    { "ArtifactToGUID", MDB_CREATE | MDB_DUPSORT, lmdb_compare_guid },      // artifact_to_guid
    { "PathToGUID", MDB_CREATE },                                           // path_to_guid
    { "TypeToGUID", MDB_CREATE | MDB_DUPSORT, lmdb_compare_guid }           // type_to_guid
)

struct PerThread
{
    DynamicArray<u8>    buffer;
    CompiledAsset       db_item;
    Path                path;
};

struct DbInfo
{
    const char*     name { nullptr };
    unsigned int    dbi { 0 };
};

struct AssetDatabaseEnv
{
    Allocator*              allocator { nullptr };
    Path                    location;
    Path                    artifacts_directory;
    MDB_env*                env { nullptr };
    unsigned int            db_maps[underlying_t(DbMapId::count)];
    FixedArray<PerThread>   per_thread;
};

void close_assetdb(AssetDatabaseEnv* env);
bool get_asset(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, CompiledAsset* asset);
bool delete_artifact(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, const u128& hash);
i32 get_artifacts_from_guid(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, AssetArtifact* dst);


unsigned int get_dbi(AssetDatabaseEnv* env, const DbMapId mapping)
{
    return env->db_maps[underlying_t(mapping)];
}

AssetDatabaseEnv* open_assetdb(const Path& directory, const StringView& name, Allocator* allocator)
{
    if (BEE_FAIL_F(directory.exists(), "Cannot open AssetDB: directory \"%s\" does not exist", directory.c_str()))
    {
        return nullptr;
    }

    auto* env = BEE_NEW(allocator, AssetDatabaseEnv);
    env->allocator = allocator;
    env->location = directory.join(name);

    if (BEE_LMDB_FAIL(mdb_env_create(&env->env)))
    {
        close_assetdb(env);
        return nullptr;
    }

    // Setup assertions and max DBI's for environment - MUST BE CONFIGURED PRIOR TO `mdb_env_open`
    const auto result = mdb_env_set_assert(env->env, &lmdb_assert_callback);
    BEE_LMDB_ASSERT(result);

    if (BEE_LMDB_FAIL(mdb_env_set_maxdbs(env->env, underlying_t(DbMapId::count))))
    {
        close_assetdb(env);
        return nullptr;
    }

    /*
     * - Default flags
     * - unix permissions (ignored on windows): -rw-rw-r--
     * - NOSUBDIR - custom database filename
     */
    if (BEE_LMDB_FAIL(mdb_env_open(env->env, env->location.c_str(), MDB_NOSUBDIR, 0664)))
    {
        close_assetdb(env);
        return nullptr;
    }

    MDB_txn* txn = nullptr;
    if (BEE_LMDB_FAIL(mdb_txn_begin(env->env, nullptr, 0, &txn)))
    {
        close_assetdb(env);
        return nullptr;
    }

    // Open handles to both databases - name map and asset storage
    bool db_map_success = true;

    for (int i = 0; i < static_array_length(env->db_maps); ++i)
    {
        const auto& info = db_mapping_info(static_cast<DbMapId>(i));
        auto* dbi = &env->db_maps[i];

        if (BEE_LMDB_FAIL(mdb_dbi_open(txn, info.name, info.flags, dbi)))
        {
            db_map_success = false;
            break;
        }

        if ((info.flags & MDB_DUPSORT) != 0)
        {
            BEE_ASSERT(info.dupsort_func != nullptr);

            if (BEE_LMDB_FAIL(mdb_set_dupsort(txn, *dbi, info.dupsort_func)))
            {
                db_map_success = false;
                break;
            }
        }
    }

    if (!db_map_success)
    {
        mdb_txn_abort(txn);
        close_assetdb(env);
        return nullptr;
    }

    const auto commit_result = mdb_txn_commit(txn);
    BEE_LMDB_ASSERT(commit_result);

    env->artifacts_directory = directory.join(g_artifacts_dirname);
    if (!env->artifacts_directory.exists())
    {
        fs::mkdir(env->artifacts_directory);
    }

    env->per_thread.resize(get_job_worker_count());
    return env;
}

void close_assetdb(AssetDatabaseEnv* env)
{
    if (env->env == nullptr)
    {
        return;
    }

    for (auto& map : env->db_maps)
    {
        if (map != g_invalid_dbi)
        {
            mdb_dbi_close(env->env, map);
        }
    }

    mdb_env_close(env->env);
    env->env = nullptr;
    BEE_DELETE(env->allocator, env);
}

DynamicArray<u8>& get_temp_buffer(AssetDatabaseEnv* env)
{
    return env->per_thread[get_local_job_worker_id()].buffer;
}

CompiledAsset& get_temp_asset(AssetDatabaseEnv* env)
{
    return env->per_thread[get_local_job_worker_id()].db_item;
}

Path& get_temp_path(AssetDatabaseEnv* env)
{
    return env->per_thread[get_local_job_worker_id()].path;
}

bool is_assetdb_open(AssetDatabaseEnv* env)
{
    return env->env != nullptr;
}

const Path& assetdb_location(AssetDatabaseEnv* env)
{
    return env->location;
}

void init_txn(AssetDatabaseEnv* env, AssetDbTxn* txn, const AssetDbTxnKind kind)
{
    if (env->env == nullptr)
    {
        return;
    }

    const auto flags = kind == AssetDbTxnKind::read_only ? MDB_RDONLY : 0;
    if (BEE_LMDB_FAIL(mdb_txn_begin(env->env, nullptr, flags, &txn->handle)))
    {
        return;
    }

    txn->kind = kind;
    txn->assetdb = &g_assetdb;
    txn->env = env;
}

AssetDbTxn read_assetdb(AssetDatabaseEnv* env)
{
    BEE_ASSERT(is_assetdb_open(env));
    AssetDbTxn txn{};
    init_txn(env, &txn, AssetDbTxnKind::read_only);
    return std::move(txn);
}

AssetDbTxn write_assetdb(AssetDatabaseEnv* env)
{
    BEE_ASSERT(is_assetdb_open(env));
    AssetDbTxn txn{};
    init_txn(env, &txn, AssetDbTxnKind::read_write);
    return std::move(txn);
}

void abort_transaction(AssetDatabaseEnv* env, AssetDbTxn* txn)
{
    if (txn == nullptr || txn->kind == AssetDbTxnKind::invalid || txn->handle == nullptr)
    {
        log_error("Invalid transaction");
        return;
    }

    mdb_txn_abort(txn->handle);

    txn->handle = nullptr;
    txn->assetdb = nullptr;
    txn->env = nullptr;
    txn->kind = AssetDbTxnKind::invalid;
}

void commit_transaction(AssetDatabaseEnv* env, AssetDbTxn* txn)
{
    if (txn == nullptr || txn->kind == AssetDbTxnKind::invalid || txn->handle == nullptr)
    {
        log_error("Invalid transaction");
        return;
    }

    mdb_txn_commit(txn->handle);

    txn->handle = nullptr;
    txn->assetdb = nullptr;
    txn->kind = AssetDbTxnKind::invalid;
}

/*
************************
*
* Asset operations
*
************************
*/
bool put_asset(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, CompiledAsset* asset)
{
    BEE_ASSERT(txn.kind == AssetDbTxnKind::read_write);

    auto& buffer = get_temp_buffer(env);

    // normalize paths
    BinarySerializer serializer(&buffer);
    serialize(SerializerMode::writing, &serializer, asset);

    auto guid_key = make_key(guid);
    auto path_key = make_key(asset->uri.view());

    MDB_val val{};

    val.mv_size = buffer.size();
    val.mv_data = buffer.data();

    // try and delete any path->guid mappings if found
    auto& old_asset_file = get_temp_asset(env);
    if (get_asset(env, txn, guid, &old_asset_file))
    {
        auto old_path_key = make_key(old_asset_file.uri.view());
        if (!txn_del(txn, get_dbi(env, DbMapId::path_to_guid), &old_path_key, nullptr))
        {
            return false;
        }
    }

    if(!txn_put(txn, get_dbi(env, DbMapId::guid_to_asset), &guid_key, &val, 0))
    {
        return false;
    }

    // add a path->guid mapping
    if (!txn_put(txn, get_dbi(env, DbMapId::path_to_guid), &path_key, &guid_key, 0))
    {
        return false;
    }

    auto type_key = make_key(asset->main_artifact.type_hash);
    LMDBCursor cursor(txn, get_dbi(env, DbMapId::type_to_guid));

    if (!cursor.get(&type_key, &guid_key, MDB_GET_BOTH))
    {
        // add a type->guid mapping
        if (!cursor.put(&type_key, &guid_key))
        {
            return false;
        }
    }

    return true;
}

bool delete_asset(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid)
{
    BEE_ASSERT(txn.kind == AssetDbTxnKind::read_write);

    auto& asset = get_temp_asset(env);
    if (!get_asset(env, txn, guid, &asset))
    {
        return false;
    }

    // Delete all the assets artifacts
    const auto artifact_count = get_artifacts_from_guid(env, txn, guid, nullptr);
    if (artifact_count > 0)
    {
        auto* artifact_hashes = BEE_ALLOCA_ARRAY(AssetArtifact, artifact_count);
        get_artifacts_from_guid(env, txn, guid, artifact_hashes);

        for (int i = 0; i < artifact_count; ++i)
        {
            if (!delete_artifact(env, txn, guid, artifact_hashes[i].content_hash))
            {
                return false;
            }
        }
    }

    auto guid_key = make_key(guid);
    MDB_val val{};

    // delete the path->guid mapping
    auto path_key = make_key(asset.uri.view());
    if (!txn_del(txn, get_dbi(env, DbMapId::path_to_guid), &path_key, nullptr))
    {
        return false;
    }

    // delete the type->guid mapping
    auto type_key = make_key(asset.main_artifact.type_hash);
    if (!txn_del(txn, get_dbi(env, DbMapId::type_to_guid), &type_key, &guid_key))
    {
        return false;
    }

    return txn_del(txn, get_dbi(env, DbMapId::guid_to_asset), &guid_key, nullptr);
}

bool get_asset(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, CompiledAsset* asset)
{
    BEE_ASSERT(asset != nullptr);

    MDB_val val{};
    auto key = make_key(guid);

    if (!txn_get(txn, get_dbi(env, DbMapId::guid_to_asset), &key, &val))
    {
        return false;
    }

    io::MemoryStream stream(val.mv_data, val.mv_size);
    StreamSerializer serializer(&stream);
    serialize(SerializerMode::reading, &serializer, asset);

    return true;
}

bool get_asset_from_path(AssetDatabaseEnv* env, const AssetDbTxn& txn, const StringView& uri, CompiledAsset* asset)
{
    // try and delete any path->guid mappings if found
    auto key = make_key(uri);
    MDB_val val{};

    if (!txn_get(txn, get_dbi(env, DbMapId::path_to_guid), &key, &val))
    {
        return false;
    }

    GUID guid{};
    memcpy(guid.data, val.mv_data, val.mv_size);

    return get_asset(env, txn, guid, asset);
}

i32 get_guids_by_type(AssetDatabaseEnv* env, const AssetDbTxn& txn, const TypeRef& type, GUID* dst)
{
    auto type_key = make_key(type->hash);
    MDB_val val{};

    LMDBCursor cursor(txn, get_dbi(env, DbMapId::type_to_guid));

    if (!cursor.get(&type_key, &val, MDB_SET_KEY))
    {
        return 0;
    }

    if (dst == nullptr)
    {
        return cursor.count();
    }

    int count = 0;

    // copy out all the GUIDs
    while (cursor.get(nullptr, &val, MDB_NEXT))
    {
        BEE_ASSERT(val.mv_size == sizeof(GUID::data));
        memcpy(dst[count].data, val.mv_data, val.mv_size);
        ++count;
    }

    return count;
}

bool has_asset(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid)
{
    auto key = make_key(guid);
    MDB_val val{};
    return txn_get(txn, get_dbi(env, DbMapId::guid_to_asset), &key, &val);
}

/*
 ************************
 *
 * Dependency operations
 *
 ************************
 */
bool set_asset_dependencies(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, const GUID* dependencies, const i32 dependency_count)
{
    auto guid_key = make_key(guid);
    MDB_val val{};

    if (txn_get(txn, get_dbi(env, DbMapId::guid_to_dependencies), &guid_key, &val))
    {
        // delete all the existing dependencies
        if (!txn_del(txn, get_dbi(env, DbMapId::guid_to_dependencies), &guid_key, nullptr))
        {
            return false;
        }
    }

    for (int i = 0; i < dependency_count; ++i)
    {
        auto dep = make_key(dependencies[i]);
        if (!txn_put(txn, get_dbi(env, DbMapId::guid_to_dependencies), &guid_key, &dep, 0))
        {
            return false;
        }
    }

    return true;
}

i32 get_asset_dependencies(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, GUID* dst)
{
    LMDBCursor cursor(txn, get_dbi(env, DbMapId::guid_to_dependencies));
    if (!cursor)
    {
        return 0;
    }

    MDB_val val{};
    auto guid_key = make_key(guid);

    if (!cursor.get(&guid_key, &val, MDB_SET_KEY))
    {
        return 0;
    }

    if (dst == nullptr)
    {
        return cursor.count();
    }

    int index = 0;

    while (cursor.get(nullptr, &val, MDB_NEXT))
    {
        BEE_ASSERT(val.mv_size == guid_key.mv_size);
        memcpy(&dst[index].data, val.mv_data, guid_key.mv_size);
        ++index;
    }

    return index;
}


/*
 ************************
 *
 * Artifact operations
 *
 ************************
 */
const Path& get_artifact_path(AssetDatabaseEnv* env, const u128& hash)
{
    static thread_local char hash_buffer[33];

    str::format_buffer(hash_buffer, static_array_length(hash_buffer), "%" BEE_PRIxu128, BEE_FMT_u128(hash));

    auto& path = get_temp_path(env);
    path.clear();
    path.append(env->artifacts_directory).append(StringView(hash_buffer, 2)).append(hash_buffer);

    return path;
}

bool put_artifact(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, const AssetArtifact& artifact, const void* buffer, const size_t buffer_size)
{
    BEE_ASSERT(txn.kind == AssetDbTxnKind::read_write);

    auto artifact_key = make_key(artifact);
    auto guid_key = make_key(guid);

    // Add a new artifact->GUID mapping
    if(!txn_put(txn, get_dbi(env, DbMapId::artifact_to_guid), &artifact_key, &guid_key, 0))
    {
        return false;
    }

    // And a mapping back from GUID->artifact
    if(!txn_put(txn, get_dbi(env, DbMapId::guid_to_artifact), &guid_key, &artifact_key, 0))
    {
        return false;
    }

    /*
     * Only write out the artifact buffer if the file doesn't already exist.
     * Because artifacts are content-hashed we can safely say that if a file already
     * exists at that path that its binary representation is exactly the same
     * TODO(Jacob): add ability to force a write-to-file in cases where the binary data
     *  has gotten corrupted somehow
     */
    const auto& artifact_path = get_artifact_path(env, artifact.content_hash);
    if (!artifact_path.exists())
    {
        const auto parent_dir = artifact_path.parent_path(temp_allocator());
        if (!parent_dir.exists())
        {
            if (!fs::mkdir(parent_dir))
            {
                return false;
            }
        }

        BEE_EXPLICIT_SCOPE
        (
            io::FileStream filestream(artifact_path, "wb");
            filestream.write(buffer, buffer_size);
        );
    }

    return true;
}

bool delete_artifact(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, const u128& hash)
{
    BEE_ASSERT(txn.kind == AssetDbTxnKind::read_write);

    auto artifact_key = make_key(AssetArtifact{hash});
    auto guid_key = make_key(guid);
    bool has_references = false;

    // Open a cursor so we can tell if this is the last guid for the artifact
    BEE_EXPLICIT_SCOPE
    (
        LMDBCursor cursor(txn, get_dbi(env, DbMapId::artifact_to_guid));
        if (!cursor && !cursor.get(&artifact_key, &guid_key, MDB_GET_BOTH))
        {
            return false;
        }

        // Delete the artifact->GUID mapping
        if (!cursor.del())
        {
            return false;
        }

        has_references = cursor.count() > 0;
    );

    // Delete the GUID->artifact mapping
    if (!txn_del(txn, get_dbi(env, DbMapId::guid_to_artifact), &guid_key, &artifact_key))
    {
        return false;
    }

    // Remove the disk artifact as well if nothing is referencing it
    if (has_references)
    {
        const auto& disk_path = get_artifact_path(env, hash);
        if (disk_path.exists())
        {
            if (!fs::remove(disk_path))
            {
                return false;
            }
        }
    }

    return true;
}

bool has_artifact(AssetDatabaseEnv* env, const AssetDbTxn& txn, const u128& hash)
{
    auto key = make_key(AssetArtifact{ hash });
    MDB_val val{};
    return txn_get(txn, get_dbi(env, DbMapId::artifact_to_guid), &key, &val);
}

bool get_artifact(AssetDatabaseEnv* env, const AssetDbTxn& txn, const u128& hash, AssetArtifact* dst, io::FileStream* dst_stream)
{
    auto artifact_key = make_key(AssetArtifact{hash});
    MDB_val val{};

    if (!has_artifact(env, txn, hash))
    {
        return false;
    }

    // copy the artifact
    BEE_ASSERT(val.mv_size == sizeof(AssetArtifact));
    memcpy(dst, val.mv_data, sizeof(AssetArtifact));

    const auto& path = get_artifact_path(env, hash);
    if (!path.exists())
    {
        return false;
    }

    dst_stream->reopen(path, "rb");
    return dst_stream->can_read();
}


i32 get_artifacts_from_guid(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, AssetArtifact* dst)
{
    LMDBCursor cursor(txn, get_dbi(env, DbMapId::guid_to_artifact));
    if (!cursor)
    {
        return false;
    }

    auto guid_key = make_key(guid);
    MDB_val val{};

    // Start at the first value in the GUID key
    if (!cursor.get(&guid_key, &val, MDB_SET_KEY))
    {
        return 0;
    }

    if (dst == nullptr)
    {
        return cursor.count();
    }

    int count = 0;
    AssetArtifact* multiple = dst;

    while (cursor.get(&guid_key, &val, MDB_NEXT_MULTIPLE))
    {
        BEE_ASSERT(val.mv_size % sizeof(AssetArtifact) == 0);

        count += static_cast<i32>(val.mv_size / sizeof(AssetArtifact));
        memcpy(multiple, val.mv_data, val.mv_size);
    }

    return count;
}

i32 get_guids_from_artifact(AssetDatabaseEnv* env, const AssetDbTxn& txn, const u128& hash, GUID* dst)
{
    LMDBCursor cursor(txn, get_dbi(env, DbMapId::artifact_to_guid));
    if (!cursor)
    {
        return false;
    }

    auto guid_key = make_key(AssetArtifact{ hash });
    MDB_val val{};

    // Start at the first value in the GUID key
    if (!cursor.get(&guid_key, &val, MDB_SET_KEY))
    {
        return 0;
    }

    if (dst == nullptr)
    {
        return cursor.count();
    }

    int count = 0;
    GUID* multiple = dst;

    while (cursor.get(&guid_key, &val, MDB_NEXT_MULTIPLE))
    {
        BEE_ASSERT(val.mv_size % sizeof(GUID) == 0);

        count += static_cast<i32>(val.mv_size / sizeof(GUID));
        memcpy(multiple, val.mv_data, val.mv_size);
    }

    return count;
}


bool runtime_locate_asset(AssetDatabaseEnv* env, const GUID& guid, AssetLocation* location)
{
    auto txn = read_assetdb(env);
    auto& local_asset = get_temp_asset(env);

    if (!get_asset(env, txn, guid, &local_asset))
    {
        return false;
    }

    location->path.clear();
//    location->path.append()
    return false;
}


static AssetLocator g_locator{};


void load_assetdb_module(PluginRegistry* registry, const PluginState state)
{
    g_assetdb.open = open_assetdb;
    g_assetdb.close = close_assetdb;
    g_assetdb.is_open = is_assetdb_open;
    g_assetdb.location = assetdb_location;
    g_assetdb.read = read_assetdb;
    g_assetdb.write = write_assetdb;
    g_assetdb.abort = abort_transaction;
    g_assetdb.commit = commit_transaction;
    g_assetdb.put_asset = put_asset;
    g_assetdb.delete_asset = delete_asset;
    g_assetdb.get_asset = get_asset;
    g_assetdb.get_asset_from_path = get_asset_from_path;
    g_assetdb.get_guids_by_type = get_guids_by_type;
    g_assetdb.has_asset = has_asset;
    g_assetdb.set_asset_dependencies = set_asset_dependencies;
    g_assetdb.get_asset_dependencies = get_asset_dependencies;
    g_assetdb.put_artifact = put_artifact;
    g_assetdb.delete_artifact = delete_artifact;
    g_assetdb.get_artifact = get_artifact;
    g_assetdb.get_artifacts_from_guid = get_artifacts_from_guid;
    g_assetdb.get_guids_from_artifact = get_guids_from_artifact;
}

} // namespace bee