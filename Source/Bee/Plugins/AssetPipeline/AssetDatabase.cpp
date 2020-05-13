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

MDB_val make_key(const u128& hash)
{
    MDB_val val{};
    val.mv_size = sizeof(u128);
    val.mv_data = const_cast<u8*>(reinterpret_cast<const u8*>(&hash));
    return val;
}

MDB_val make_key(const GUID& guid)
{
    MDB_val val{};
    val.mv_size = static_array_length(guid.data);
    val.mv_data = const_cast<u8*>(guid.data);
    return val;
}


/*
 ********************************
 *
 * Database - implementation
 *
 ********************************
 */
enum class DbMapping
{
    guid_to_asset,
    guid_to_name,
    name_to_guid,
    path_to_guid,
    artifact_hash_to_path,
    count
};

BEE_TRANSLATION_TABLE(db_mapping_name, DbMapping, const char*, DbMapping::count,
    "GUIDToAsset",          // guid_to_asset
    "GUIDToName",           // guid_to_name
    "NameToGUID",           // name_to_guid
    "PathToGUID",           // path_to_guid
    "ArtifactHashToPath"    // artifact_hash_to_path
)

struct PerThread
{
    DynamicArray<u8>    buffer;
    AssetDbItem         db_item;
    Path                path;
};

struct DbInfo
{
    const char*     name { nullptr };
    unsigned int    dbi { 0 };
};

struct AssetDb
{
    Path                    location;
    Path                    artifacts_directory;
    MDB_env*                env { nullptr };
    unsigned int            db_maps[underlying_t(DbMapping::count)];
    FixedArray<PerThread>   per_thread;
};

static AssetDb*     g_assetdb { nullptr };
AssetDatabaseModule g_assetdb_module{};


void close_assetdb();
bool set_asset_name(const AssetDbTxn& txn, const GUID& guid, const StringView& name);
bool get_asset(const AssetDbTxn& txn, const GUID& guid, AssetDbItem* asset);
bool delete_artifact(const AssetDbTxn& txn, const u128& hash);

unsigned int get_dbi(const DbMapping mapping)
{
    return g_assetdb->db_maps[underlying_t(mapping)];
}

void open_assetdb(const Path& directory, const StringView& name)
{
    if (BEE_FAIL_F(g_assetdb->env == nullptr, "AssetDB is already opened at path: %s", g_assetdb->location.c_str()))
    {
        return;
    }

    if (BEE_FAIL_F(directory.exists(), "Cannot open AssetDB: directory \"%s\" does not exist", directory.c_str()))
    {
        return;
    }

    g_assetdb->location = directory.join(name);

    if (BEE_LMDB_FAIL(mdb_env_create(&g_assetdb->env)))
    {
        close_assetdb();
        return;
    }

    // Setup assertions and max DBI's for environment - MUST BE CONFIGURED PRIOR TO `mdb_env_open`
    const auto result = mdb_env_set_assert(g_assetdb->env, &lmdb_assert_callback);
    BEE_LMDB_ASSERT(result);

    if (BEE_LMDB_FAIL(mdb_env_set_maxdbs(g_assetdb->env, underlying_t(DbMapping::count))))
    {
        close_assetdb();
        return;
    }

    /*
     * - Default flags
     * - unix permissions (ignored on windows): -rw-rw-r--
     * - NOSUBDIR - custom database filename
     */
    if (BEE_LMDB_FAIL(mdb_env_open(g_assetdb->env, g_assetdb->location.c_str(), MDB_NOSUBDIR, 0664)))
    {
        close_assetdb();
        return;
    }

    MDB_txn* txn = nullptr;
    if (BEE_LMDB_FAIL(mdb_txn_begin(g_assetdb->env, nullptr, 0, &txn)))
    {
        return;
    }

    // Open handles to both databases - name map and asset storage
    bool db_map_success = true;

    for (int i = 0; i < static_array_length(g_assetdb->db_maps); ++i)
    {
        const auto* map_name = db_mapping_name(static_cast<DbMapping>(i));
        if (BEE_LMDB_FAIL(mdb_dbi_open(txn, map_name, MDB_CREATE, &g_assetdb->db_maps[i])))
        {
            db_map_success = false;
            break;
        }
    }

    if (!db_map_success)
    {
        mdb_txn_abort(txn);
        close_assetdb();
        return;
    }

    const auto commit_result = mdb_txn_commit(txn);
    BEE_LMDB_ASSERT(commit_result);

    g_assetdb->artifacts_directory = directory.join(g_artifacts_dirname);
    if (!g_assetdb->artifacts_directory.exists())
    {
        fs::mkdir(g_assetdb->artifacts_directory);
    }

    g_assetdb->per_thread.resize(get_job_worker_count());
}

void close_assetdb()
{
    if (g_assetdb->env == nullptr)
    {
        return;
    }

    for (auto& map : g_assetdb->db_maps)
    {
        if (map != g_invalid_dbi)
        {
            mdb_dbi_close(g_assetdb->env, map);
        }
    }

    mdb_env_close(g_assetdb->env);
    g_assetdb->env = nullptr;
}

DynamicArray<u8>& db_local_buffer()
{
    return g_assetdb->per_thread[get_local_job_worker_id()].buffer;
}

AssetDbItem& db_local_item()
{
    return g_assetdb->per_thread[get_local_job_worker_id()].db_item;
}

Path& db_local_path()
{
    return g_assetdb->per_thread[get_local_job_worker_id()].path;
}

bool is_assetdb_open()
{
    return g_assetdb->env != nullptr;
}

const Path& assetdb_location()
{
    return g_assetdb->location;
}

void init_txn(AssetDbTxn* txn, const AssetDbTxnKind kind)
{
    if (g_assetdb->env == nullptr)
    {
        return;
    }

    const auto flags = kind == AssetDbTxnKind::read_only ? MDB_RDONLY : 0;
    if (BEE_LMDB_FAIL(mdb_txn_begin(g_assetdb->env, nullptr, flags, &txn->handle)))
    {
        return;
    }

    txn->kind = kind;
    txn->assetdb = &g_assetdb_module;
}

AssetDbTxn read_assetdb()
{
    BEE_ASSERT(is_assetdb_open());
    AssetDbTxn txn{};
    init_txn(&txn, AssetDbTxnKind::read_only);
    return std::move(txn);
}

AssetDbTxn write_assetdb()
{
    BEE_ASSERT(is_assetdb_open());
    AssetDbTxn txn{};
    init_txn(&txn, AssetDbTxnKind::read_write);
    return std::move(txn);
}

void abort_transaction(AssetDbTxn* txn)
{
    if (txn == nullptr || txn->kind == AssetDbTxnKind::invalid || txn->handle == nullptr)
    {
        log_error("Invalid transaction");
        return;
    }

    mdb_txn_abort(txn->handle);

    txn->handle = nullptr;
    txn->assetdb = nullptr;
    txn->kind = AssetDbTxnKind::invalid;
}

void commit_transaction(AssetDbTxn* txn)
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

bool put_asset(const AssetDbTxn& txn, AssetDbItem* asset)
{
    BEE_ASSERT(txn.kind == AssetDbTxnKind::read_write);

    auto& buffer = db_local_buffer();

    // normalize paths
    asset->contents.source.make_generic().set_extension("");

    BinarySerializer serializer(&buffer);
    serialize(SerializerMode::writing, &serializer, asset);

    auto guid_key = make_key(asset->contents.guid);
    auto path_key = make_key(asset->contents.source.view());

    MDB_val val{};

    // Put the actual asset file data into the DB
    val.mv_size = buffer.size();
    val.mv_data = buffer.data();

    // try and delete any path->guid mappings if found
    auto& old_asset_file = db_local_item();
    if (get_asset(txn, asset->contents.guid, &old_asset_file))
    {
        auto old_path_key = make_key(old_asset_file.contents.source.view());
        if (BEE_LMDB_FAIL(mdb_del(txn.handle, get_dbi(DbMapping::path_to_guid), &old_path_key, nullptr)))
        {
            return false;
        }
    }

    if(BEE_LMDB_FAIL(mdb_put(txn.handle, get_dbi(DbMapping::guid_to_asset), &guid_key, &val, 0)))
    {
        return false;
    }

    // add a path->guid mapping
    if (BEE_LMDB_FAIL(mdb_put(txn.handle, get_dbi(DbMapping::path_to_guid), &path_key, &guid_key, 0)))
    {
        return false;
    }

    return set_asset_name(txn, asset->contents.guid, asset->contents.name.view());
}

bool delete_asset(const AssetDbTxn& txn, const GUID& guid)
{
    BEE_ASSERT(txn.kind == AssetDbTxnKind::read_write);

    auto& asset = db_local_item();
    if (!get_asset(txn, guid, &asset))
    {
        return false;
    }

    // Delete all the assets artifacts
    for (auto& hash : asset.contents.artifacts)
    {
        if (!delete_artifact(txn, hash))
        {
            return false;
        }
    }

    auto guid_key = make_key(guid);
    MDB_val val{};

    auto result = mdb_get(txn.handle, get_dbi(DbMapping::guid_to_name), &guid_key, &val);

    // Delete the GUID->Name & Name->GUID mappings if they exist
    if (result != MDB_NOTFOUND)
    {
        if (BEE_LMDB_FAIL(result))
        {
            return false;
        }

        if (BEE_LMDB_FAIL(mdb_del(txn.handle, get_dbi(DbMapping::name_to_guid), &val, nullptr)))
        {
            return false;
        }

        if (BEE_LMDB_FAIL(mdb_del(txn.handle, get_dbi(DbMapping::guid_to_name), &val, nullptr)))
        {
            return false;
        }
    }

    // delete the path->guid mapping
    auto path_key = make_key(asset.contents.source.view());
    if (BEE_LMDB_FAIL(mdb_del(txn.handle, get_dbi(DbMapping::path_to_guid), &path_key, nullptr)))
    {
        return false;
    }

    result = mdb_del(txn.handle, get_dbi(DbMapping::guid_to_asset), &guid_key, nullptr);
    return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
}

bool get_asset(const AssetDbTxn& txn, const GUID& guid, AssetDbItem* asset)
{
    BEE_ASSERT(asset != nullptr);

    MDB_val val{};
    auto key = make_key(guid);

    const auto result = mdb_get(txn.handle, get_dbi(DbMapping::guid_to_asset), &key, &val);

    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    io::MemoryStream stream(val.mv_data, val.mv_size);
    StreamSerializer serializer(&stream);
    serialize(SerializerMode::reading, &serializer, asset);

    return true;
}

bool get_asset_from_path(const AssetDbTxn& txn, const Path& normalized_path, AssetDbItem* asset)
{
    // try and delete any path->guid mappings if found
    auto& generic_path = db_local_path();
    generic_path.clear();
    generic_path.append(normalized_path).make_generic().set_extension("");

    auto key = make_key(generic_path.view());
    MDB_val val{};

    const auto result = mdb_get(txn.handle, get_dbi(DbMapping::path_to_guid), &key, &val);
    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    GUID guid{};
    memcpy(guid.data, val.mv_data, val.mv_size);

    return get_asset(txn, guid, asset);
}

bool has_asset(const AssetDbTxn& txn, const GUID& guid)
{
    auto key = make_key(guid);
    MDB_val val{};
    const auto result = mdb_get(txn.handle, get_dbi(DbMapping::guid_to_asset), &key, &val);
    return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
}

bool set_asset_name(const AssetDbTxn& txn, const GUID& guid, const StringView& name)
{
    BEE_ASSERT(txn.kind == AssetDbTxnKind::read_write);

    // TODO(JACOB): delete from the stored asset file as well
    if (!has_asset(txn, guid))
    {
        log_error("No such asset %s", format_guid(guid, GUIDFormat::digits));
        return false;
    }

    auto guid_key = make_key(guid);
    MDB_val val{};

    const auto result = mdb_get(txn.handle, get_dbi(DbMapping::guid_to_name), &guid_key, &val);
    if (result != MDB_NOTFOUND && BEE_LMDB_FAIL(result))
    {
        return false;
    }

    if (result != MDB_NOTFOUND)
    {
        if (name != StringView(static_cast<const char*>(val.mv_data), val.mv_size))
        {
            // If the name has changed then we need to update the maps
            // Delete the old name
            if (BEE_LMDB_FAIL(mdb_del(txn.handle, get_dbi(DbMapping::name_to_guid), &val, nullptr)))
            {
                return false;
            }
        }

        // delete the name entirely if the it's empty
        if (name.empty())
        {
            return !BEE_LMDB_FAIL(mdb_del(txn.handle, get_dbi(DbMapping::guid_to_name), &guid_key, nullptr));
        }
    }

    if (!name.empty())
    {
        // Otherwise update the maps
        val = make_key(name);

        // Update the GUID->name DB with the new name
        if (BEE_LMDB_FAIL(mdb_put(txn.handle, get_dbi(DbMapping::guid_to_name), &guid_key, &val, 0)))
        {
            return false;
        }

        // Update the Name->GUID DB with the new name
        if (BEE_LMDB_FAIL(mdb_put(txn.handle, get_dbi(DbMapping::name_to_guid), &val, &guid_key, 0)))
        {
            return false;
        }

    }

    return true;
}

bool get_name_from_guid(const AssetDbTxn& txn, const GUID& guid, String* name)
{
    auto key = make_key(guid);
    MDB_val val{};

    const auto result = mdb_get(txn.handle, get_dbi(DbMapping::guid_to_name), &key, &val);
    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    name->append(StringView(static_cast<const char*>(val.mv_data), val.mv_size));
    return true;
}

bool get_guid_from_name(const AssetDbTxn& txn, const StringView& name, GUID* guid)
{
    auto key = make_key(name);
    MDB_val val{};

    const auto result = mdb_get(txn.handle, get_dbi(DbMapping::name_to_guid), &key, &val);
    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    BEE_ASSERT(val.mv_size == static_array_length(guid->data));

    memcpy(guid->data, val.mv_data, val.mv_size);

    return true;
}

const Path& get_artifact_path(const u128& hash)
{
    static thread_local char hash_buffer[33];

    str::format_buffer(hash_buffer, static_array_length(hash_buffer), "%" BEE_PRIxu128, BEE_FMT_u128(hash));

    auto& path = db_local_path();
    path.clear();
    path.append(g_assetdb->artifacts_directory).append(StringView(hash_buffer, 2)).append(hash_buffer);

    return path;
}

bool put_artifact(const AssetDbTxn& txn, const AssetArtifact& artifact)
{
    BEE_ASSERT(txn.kind == AssetDbTxnKind::read_write);

    auto key = make_key(artifact.content_hash);
    MDB_val val{};

    // Put the actual asset file data into the DB
    const auto& path = get_artifact_path(artifact.content_hash);
    val.mv_size = path.size();
    val.mv_data = const_cast<void*>(static_cast<const void*>(path.c_str()));

    if(BEE_LMDB_FAIL(mdb_put(txn.handle, get_dbi(DbMapping::artifact_hash_to_path), &key, &val, 0)))
    {
        return false;
    }

    // HACK(Jacob): fix this when path_view is implemented
    const auto parent_dir = path.parent_path(temp_allocator());
    if (!parent_dir.exists())
    {
        fs::mkdir(parent_dir);
    }

    fs::write(path, artifact.buffer.const_span());
    return true;
}

bool delete_artifact(const AssetDbTxn& txn, const u128& hash)
{
    BEE_ASSERT(txn.kind == AssetDbTxnKind::read_write);

    auto key = make_key(hash);
    MDB_val val{};

    auto result = mdb_get(txn.handle, get_dbi(DbMapping::guid_to_name), &key, &val);
    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    // Remove the disk artifact as well
    Path path(StringView(static_cast<const char*>(val.mv_data), val.mv_size));

    if (path.exists())
    {
        fs::remove(path);
    }

    result = mdb_del(txn.handle, get_dbi(DbMapping::artifact_hash_to_path), &key, nullptr);
    return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
}

bool get_artifact_path(const AssetDbTxn& txn, const u128& hash, Path* dst)
{
    auto key = make_key(hash);
    MDB_val val{};

    const auto result = mdb_get(txn.handle, get_dbi(DbMapping::guid_to_name), &key, &val);
    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    dst->clear();
    dst->append(StringView(static_cast<const char*>(val.mv_data), val.mv_size));
    return true;
}

bool get_artifact(const AssetDbTxn& txn, const u128& hash, AssetArtifact* artifact)
{
    Path path(temp_allocator());
    if (!get_artifact_path(txn, hash, &path))
    {
        return false;
    }

    io::FileStream stream(path, "rb");
    artifact->content_hash = hash;
    artifact->buffer.resize(stream.size());
    stream.read(artifact->buffer.data(), artifact->buffer.size());
    return true;
}

bool get_artifacts_from_guid(const AssetDbTxn& txn, const GUID& guid, DynamicArray<AssetArtifact>* result)
{
    auto& asset = db_local_item();
    if (!get_asset(txn, guid, &asset))
    {
        return false;
    }

    AssetArtifact artifact{};

    for (auto& hash : asset.contents.artifacts)
    {
        if (!get_artifact(txn, hash, &artifact))
        {
            log_error(
                "Missing or invalid artifact hash %s found for asset with GUID %s",
                str::to_string(hash, temp_allocator()).c_str(),
                format_guid(guid, GUIDFormat::digits)
            );

            return false;
        }

        result->push_back(artifact);
    }

    return true;
}


bool runtime_locate_asset(const GUID& guid, AssetLocation* location)
{
    auto txn = read_assetdb();
    auto& local_asset = db_local_item();

    if (!get_asset(txn, guid, &local_asset))
    {
        return false;
    }

    location->path.clear();
    location->path.append()
}

bool runtime_locate_asset_by_name(const StringView& name, GUID* dst, AssetLocation* location)
{

}

static AssetLocator g_locator{};


void load_assetdb_module(PluginRegistry* registry, const PluginState state)
{
    g_assetdb = registry->get_or_create_persistent<AssetDb>("BeeAssetDatabase");

    g_assetdb_module.open = open_assetdb;
    g_assetdb_module.close = close_assetdb;
    g_assetdb_module.is_open = is_assetdb_open;
    g_assetdb_module.location = assetdb_location;
    g_assetdb_module.read = read_assetdb;
    g_assetdb_module.write = write_assetdb;
    g_assetdb_module.abort_transaction = abort_transaction;
    g_assetdb_module.commit_transaction = commit_transaction;
    g_assetdb_module.put_asset = put_asset;
    g_assetdb_module.delete_asset = delete_asset;
    g_assetdb_module.get_asset = get_asset;
    g_assetdb_module.get_asset_from_path = get_asset_from_path;
    g_assetdb_module.has_asset = has_asset;
    g_assetdb_module.set_asset_name = set_asset_name;
    g_assetdb_module.get_name_from_guid = get_name_from_guid;
    g_assetdb_module.get_guid_from_name = get_guid_from_name;
    g_assetdb_module.put_artifact = put_artifact;
    g_assetdb_module.delete_artifact = delete_artifact;
    g_assetdb_module.get_artifact = get_artifact;
    g_assetdb_module.get_artifacts_from_guid = get_artifacts_from_guid;
}

} // namespace bee