/*
 *  AssetDatabase.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetDatabase/AssetDatabase.inl"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Serialization/BinarySerializer.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "AssetDatabase.hpp"


#include <lmdb.h>


static bee::AssetDatabaseModule g_module{};


namespace bee {

/*
 ******************************************
 *
 * LMDB maps - manages the individual
 * mdb dbi's for a given assetdb
 *
 ******************************************
 */
static constexpr auto g_invalid_dbi = limits::max<u32>();

struct DbMapInfo
{
    const char*     name { nullptr };
    unsigned int    flags { 0 };
    MDB_cmp_func*   dupsort_func { nullptr };
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
    const auto* lhs = static_cast<const AssetArtifact*>(a->mv_data);
    const auto* rhs = static_cast<const AssetArtifact*>(b->mv_data);

    if (lhs->content_hash < rhs->content_hash)
    {
        return -1;
    }

    if (lhs->content_hash > rhs->content_hash)
    {
        return 1;
    }

    return 0;
}

BEE_TRANSLATION_TABLE_FUNC(db_mapping_info, DbMapId, DbMapInfo, DbMapId::count,
//    { "URIToPath",              MDB_CREATE },                                               // uri_to_path
    { "GUIDToAsset",            MDB_CREATE },                                               // guid_to_asset
    { "GUIDToDependencies", MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, lmdb_compare_guid },  // guid_to_dependencies
    { "GUIDToArtifacts", MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, lmdb_compare_artifact },  // guid_to_artifact
    { "ArtifactToGUID", MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, lmdb_compare_guid },      // artifact_to_guid
//    { "PathToGUID", MDB_CREATE },                                                          // path_to_guid
//    { "TypeToGUID",             MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, lmdb_compare_guid }                // type_to_guid
)

/*
 ***************************
 *
 * LMDB API helpers
 *
 ***************************
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

MDB_val make_key(const u128& hash)
{
    MDB_val val{};
    val.mv_size = sizeof(u128);
    val.mv_data = reinterpret_cast<u8*>(const_cast<u128*>(&hash));
    return val;
}

bool basic_txn_get(MDB_txn* txn, const unsigned int dbi, MDB_val* key, MDB_val* val)
{
    const auto result = mdb_get(txn, dbi, key, val);
    return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
}

bool basic_txn_del(MDB_txn* txn, const unsigned int dbi, MDB_val* key, MDB_val* val)
{
    const auto result = mdb_del(txn, dbi, key, val);
    return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
}

bool basic_txn_put(MDB_txn* txn, const unsigned int dbi, MDB_val* key, MDB_val* val, const unsigned int flags)
{
    const auto result = mdb_put(txn, dbi, key, val, flags);
    return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
}

struct LMDBCursor
{
    MDB_cursor* cursor { nullptr };

    LMDBCursor(AssetTxnData* txn, const MDB_dbi dbi)
    {
        if (BEE_LMDB_FAIL(mdb_cursor_open(txn->handle, dbi, &cursor)))
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

/*
 *************************************
 *
 * Database and scoped alloc
 *
 *************************************
 */
void asset_txn_init(AssetTxnData* txn, AssetDatabase* db, const AssetTxnAccess access);

static void db_thread_init(ThreadData* thread)
{
    new (&thread->txn_allocator) ChunkAllocator(megabytes(2), 64, 0);
    new (&thread->tmp_allocator) LinearAllocator(megabytes(8), system_allocator());
}

static ThreadData& db_get_thread(AssetDatabase* db)
{
    return db->thread_data[job_worker_id()];
}

static unsigned int db_get_dbi(AssetDatabase* db, const DbMapId id)
{
    return db->db_maps[underlying_t(id)];
}

static void db_txn_list_append(AssetTxnData** list, AssetTxnData* item)
{
    if (*list == nullptr)
    {
        *list = item;
    }
    else
    {
        (*list)->prev = item;
        item->next = *list;
        *list = item;
    }
}

static void db_txn_list_remove(AssetTxnData* item)
{
    if (item->prev != nullptr)
    {
        item->prev->next = item->next;
    }
    if (item->next != nullptr)
    {
        item->next->prev = item->prev;
    }

    item->prev = item->next = nullptr;
}

static AssetTxnData* db_create_txn(AssetDatabase* db, const AssetTxnAccess access)
{
    auto& thread = db_get_thread(db);
    auto txn = BEE_NEW(thread.txn_allocator, AssetTxnData);
    txn->thread = job_worker_id();
    txn->db = db;
    txn->access = access;
    txn->allocator = &thread.txn_allocator;

    if (BEE_LMDB_FAIL(mdb_txn_begin(db->env, nullptr, access == AssetTxnAccess::read_only ? MDB_RDONLY : 0, &txn->handle)))
    {
        BEE_DELETE(thread.txn_allocator, txn);
        return nullptr;
    }

    db_txn_list_append(&thread.transactions, txn);
    return txn;
}

BEE_TRANSLATION_TABLE_FUNC(db_code_to_string, AssetDatabaseStatus, const char*, AssetDatabaseStatus::internal_error,
    "Transaction has reached the maximum number asset modifications and creations",
    "Asset properties handle was invalid",
    "Asset properties handle points to a deleted asset",
    "Attempted to modify an asset in a read-only transaction",
    "Asset not found",
    "Failed to write artifact buffer to disk"
);

static AssetDatabaseError make_error(const AssetDatabaseStatus code)
{
    if (underlying_t(code) >= underlying_t(AssetDatabaseStatus::internal_error))
    {
        return { code, mdb_strerror(underlying_t(code)) };
    }

    return { code, db_code_to_string(code) };
}

TempAllocScope::TempAllocScope(AssetDatabase* db)
    : thread(&db->thread_data[job_worker_id()])
{
    offset = thread->tmp_allocator.offset();
}

TempAllocScope::~TempAllocScope()
{
    thread->tmp_allocator.reset_offset(offset);
}

/*
 *************************************
 *
 * Transaction types
 *
 *************************************
 */
static bool asset_txn_is_valid(AssetTxnData* txn)
{
    return txn->handle != nullptr;
}

void get_artifact_path(AssetTxn* txn, const u128& hash, Path* dst)
{
    StaticString<32> hash_string;
    str::to_static_string(hash, &hash_string);

    const auto dir = str::substring(hash_string.view(), 0, 2);
    dst->append(txn->data()->db->artifacts_root.view()).append(dir).append(hash_string.view());
}

static bool asset_txn_get(AssetTxnData* txn, const DbMapId id, const GUID& key, AssetMetadata* meta)
{
    BEE_ASSERT(asset_txn_is_valid(txn));
    auto mdb_key = make_key(key);
    MDB_val mdb_val{};
    if (!basic_txn_get(txn->handle, db_get_dbi(txn->db, id), &mdb_key, &mdb_val))
    {
        return false;
    }

    if (meta != nullptr)
    {
        io::MemoryStream stream(static_cast<u8*>(mdb_val.mv_data), mdb_val.mv_size);
        StreamSerializer serializer(&stream);
        serialize(SerializerMode::reading, &serializer, meta, txn->allocator);
    }

    return true;
}

static bool asset_txn_del(AssetTxnData* txn, const GUID& key)
{
    BEE_ASSERT(asset_txn_is_valid(txn));
    auto mdb_key = make_key(key);
    return basic_txn_del(txn->handle, db_get_dbi(txn->db, DbMapId::guid_to_asset), &mdb_key, nullptr);
}

static bool asset_txn_put(AssetTxnData* txn, AssetMetadata* meta, const unsigned int flags)
{
    BEE_ASSERT(asset_txn_is_valid(txn));
    auto& thread = db_get_thread(txn->db);

    BinarySerializer serializer(&thread.tmp_buffer);
    serialize(SerializerMode::writing, &serializer, meta);

    auto mdb_key = make_key(meta->guid);
    MDB_val mdb_val{};
    mdb_val.mv_data = thread.tmp_buffer.data();
    mdb_val.mv_size = thread.tmp_buffer.size();
    return basic_txn_put(txn->handle, db_get_dbi(txn->db, DbMapId::guid_to_asset), &mdb_key, &mdb_val, flags);
}

static bool asset_txn_del(AssetTxnData* txn, const GUID& key, const u128& hash, i32* artifact_count)
{
    auto mdb_guid = make_key(key);
    auto mdb_hash = make_key(hash);

    {
        // Find the hash->GUID mapping and delete it
        LMDBCursor cursor(txn, db_get_dbi(txn->db, DbMapId::artifact_to_guid));
        if (!cursor && !cursor.get(&mdb_hash, &mdb_guid, MDB_GET_BOTH))
        {
            return false;
        }

        if (!cursor.del())
        {
            return false;
        }

        // delete the disk asset if nothing is referencing this artifact
        if (artifact_count != nullptr)
        {
            *artifact_count = cursor.count();
        }
    }

    // Delete the GUID->artifact mapping
    return basic_txn_del(txn->handle, db_get_dbi(txn->db, DbMapId::guid_to_artifacts), &mdb_guid, &mdb_hash);
}

static bool asset_txn_put(AssetTxnData* txn, const GUID& key, const u128& hash, const u32 type, const unsigned int flags)
{
    BEE_ASSERT(asset_txn_is_valid(txn));

    AssetArtifact artifact{};
    artifact.type_hash = type;
    artifact.content_hash = hash;

    auto mdb_key = make_key(key);
    MDB_val mdb_val{};
    mdb_val.mv_data = &artifact;
    mdb_val.mv_size = sizeof(AssetArtifact);

    // Put the guid->artifact mapping in
    if (!basic_txn_put(txn->handle, db_get_dbi(txn->db, DbMapId::guid_to_artifacts), &mdb_key, &mdb_val, flags))
    {
        return false;
    }

    // Map back from hash->guid
    auto hash_key = make_key(hash);
    if (!basic_txn_put(txn->handle, db_get_dbi(txn->db, DbMapId::artifact_to_guid), &hash_key, &mdb_key, flags))
    {
        return false;
    }

    return true;
}

ScopedTxn::ScopedTxn(AssetDatabase* new_db, const unsigned int flags)
    : db(new_db)
{
    if (BEE_LMDB_FAIL(mdb_txn_begin(new_db->env, nullptr, flags, &txn)))
    {
        txn = nullptr;
    }
}

ScopedTxn::~ScopedTxn()
{
    BEE_LMDB_ASSERT(mdb_txn_commit(txn));
}

bool ScopedTxn::get(const DbMapId id, MDB_val* key, MDB_val* val)
{
    return basic_txn_get(txn, db_get_dbi(db, id), key, val);
}

bool ScopedTxn::del(const DbMapId id, MDB_val* key, MDB_val* val)
{
    return basic_txn_del(txn, db_get_dbi(db, id), key, val);
}

bool ScopedTxn::put(const DbMapId id, MDB_val* key, MDB_val* val, const unsigned int flags)
{
    return basic_txn_put(txn, db_get_dbi(db, id), key, val, flags);
}

/*
 *************************************
 *
 * AssetDatabase - implementation
 *
 *************************************
 */
void close(AssetDatabase* db);

AssetDatabase* open(const Path& location)
{
    const auto dir = location.parent_path();

    if (BEE_FAIL_F(dir.exists(), "Cannot open AssetDB: directory \"%s\" does not exist", dir.c_str()))
    {
        return nullptr;
    }

    auto* db = BEE_NEW(system_allocator(), AssetDatabase);
    db->location.append(location);
    db->artifacts_root.append(location.parent_view()).append("Artifacts");
    db->thread_data.resize(job_system_worker_count());

    for (auto& thread : db->thread_data)
    {
        db_thread_init(&thread);
    }

    if (BEE_LMDB_FAIL(mdb_env_create(&db->env)))
    {
        close(db);
        return nullptr;
    }

    // Setup assertions and max DBI's for environment - MUST BE CONFIGURED PRIOR TO `mdb_env_open`
    const auto result = mdb_env_set_assert(db->env, &lmdb_assert_callback);
    BEE_LMDB_ASSERT(result);

    if (BEE_LMDB_FAIL(mdb_env_set_maxdbs(db->env, underlying_t(DbMapId::count))))
    {
        close(db);
        return nullptr;
    }

    /*
     * - Default flags
     * - unix permissions (ignored on windows): -rw-rw-r--
     * - NOSUBDIR - custom database filename
     */
    if (BEE_LMDB_FAIL(mdb_env_open(db->env, db->location.c_str(), MDB_NOSUBDIR, 0664)))
    {
        close(db);
        return nullptr;
    }

    MDB_txn* txn = nullptr;
    if (BEE_LMDB_FAIL(mdb_txn_begin(db->env, nullptr, 0, &txn)))
    {
        close(db);
        return nullptr;
    }

    // Open handles to both databases - name map and asset storage
    bool db_map_success = true;

    for (int i = 0; i < static_array_length(db->db_maps); ++i)
    {
        const auto& info = db_mapping_info(static_cast<DbMapId>(i));
        auto* dbi = &db->db_maps[i];

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
        close(db);
        return nullptr;
    }

    const auto commit_result = mdb_txn_commit(txn);
    BEE_LMDB_ASSERT(commit_result);
    return db;
}

void close(AssetDatabase* db)
{
    if (db->env == nullptr)
    {
        return;
    }

    for (auto& map : db->db_maps)
    {
        if (map != g_invalid_dbi)
        {
            mdb_dbi_close(db->env, map);
        }
    }

    mdb_env_close(db->env);
    db->env = nullptr;
    BEE_DELETE(system_allocator(), db);
}

bool is_open(AssetDatabase* db)
{
    return db->env != nullptr;
}

const Path& location(AssetDatabase* db)
{
    return db->location;
}

void gc(AssetDatabase* db)
{
    scoped_recursive_lock_t lock(db->gc_mutex);

    for (auto& thread : db->thread_data)
    {
        auto* txn = thread.gc_transactions;
        while (txn != nullptr)
        {
            auto* next = txn->next;
            BEE_DELETE(thread.txn_allocator, txn);
            txn = next;
        }
        thread.gc_transactions = nullptr;
    }
}

AssetTxn read(AssetDatabase* db)
{
    return { &g_module, db_create_txn(db, AssetTxnAccess::read_only) };
}

AssetTxn write(AssetDatabase* db)
{
    return { &g_module, db_create_txn(db, AssetTxnAccess::read_write) };
}

void abort(AssetTxn* txn)
{
    auto* txn_data = txn->data();

    mdb_txn_abort(txn_data->handle);
    txn_data->handle = nullptr;

    db_txn_list_remove(txn_data);
    db_txn_list_append(&txn_data->db->thread_data[txn_data->thread].gc_transactions, txn_data);
}

bool commit(AssetTxn* txn)
{
    auto* txn_data = txn->data();
    // TODO(Jacob): write all the pending data
    for (int i = 0; i < txn->data()->asset_count; ++i)
    {
        if (txn_data->asset_op[i] != AssetOp::modify && txn_data->asset_op[i] != AssetOp::create)
        {
            continue;
        }

        if (!asset_txn_put(txn_data, &txn_data->asset_metadata[i], 0))
        {
            return false;
        }
    }

    BEE_LMDB_ASSERT(mdb_txn_commit(txn_data->handle));
    txn_data->handle = nullptr;

    db_txn_list_remove(txn_data);
    db_txn_list_append(&txn_data->db->thread_data[txn_data->thread].gc_transactions, txn_data);

    return true;
}

bool asset_exists(AssetTxn* txn, const GUID guid)
{
    if (asset_txn_get(txn->data(), DbMapId::guid_to_asset, guid, nullptr))
    {
        return true;
    }

    const int pending = find_index_if(txn->data()->asset_metadata, txn->data()->asset_metadata + txn->data()->asset_count, [&](const AssetMetadata& meta)
    {
        return meta.guid == guid;
    });

    return pending >= 0;
}

static Result<i32, AssetDatabaseError> allocate_asset(AssetTxnData* txn, const GUID guid)
{
    if (txn->asset_count >= BEE_ASSET_TXN_MAX_ASSETS)
    {
        return make_error(AssetDatabaseStatus::txn_max_asset_ops);
    }

    int index = find_index_if(txn->asset_metadata, txn->asset_metadata + txn->asset_count, [&](const AssetMetadata& meta)
    {
        return meta.guid == guid;
    });

    if (index >= 0)
    {
        return index;
    }

    index = txn->asset_count;

    if (!asset_txn_get(txn, DbMapId::guid_to_asset, guid, &txn->asset_metadata[index]))
    {
        return make_error(AssetDatabaseStatus::not_found);
    }

    ++txn->asset_count;
    txn->asset_metadata[index].guid = guid;
    txn->asset_op[index] = AssetOp::read;
    return index;
}

Result<AssetMetadata*, AssetDatabaseError> create_asset(AssetTxn* txn, const Type type)
{
    auto* txn_data = txn->data();

    if (txn_data->asset_count >= BEE_ASSET_TXN_MAX_ASSETS)
    {
        return make_error(AssetDatabaseStatus::txn_max_asset_ops);
    }

    const int index = txn_data->asset_count;
    ++txn_data->asset_count;

    txn_data->asset_op[index] = AssetOp::create;

    auto& meta = txn_data->asset_metadata[index];
    meta.guid = generate_guid();
    meta.kind = AssetFileKind::file;
    meta.properties = type->create_instance(txn_data->allocator);
    return &meta;
}

bool delete_asset(AssetTxn* txn, const GUID guid)
{
    auto* txn_data = txn->data();

    int index = find_index_if(txn_data->asset_metadata, txn_data->asset_metadata + txn_data->asset_count, [&](const AssetMetadata& meta)
    {
        return meta.guid == guid;
    });

    if (index >= 0 && txn_data->asset_op[index] == AssetOp::create)
    {
        txn_data->asset_op[index] = AssetOp::deleted;
        return true;
    }

    if (!asset_exists(txn, guid))
    {
        return false;
    }

    return asset_txn_del(txn_data, guid);
}

Result<const AssetMetadata*, AssetDatabaseError> read_asset(AssetTxn* txn, const GUID guid)
{
    auto res = allocate_asset(txn->data(), guid);
    if (!res)
    {
        return res.unwrap_error();
    }

    return &txn->data()->asset_metadata[res.unwrap()];
}

Result<AssetMetadata*, AssetDatabaseError> modify_asset(AssetTxn* txn, const GUID guid)
{
    auto* txn_data = txn->data();
    if (txn_data->access != AssetTxnAccess::read_write)
    {
        return make_error(AssetDatabaseStatus::invalid_access);
    }

    auto res = allocate_asset(txn_data, guid);
    if (!res)
    {
        return res.unwrap_error();
    }

    const int index = res.unwrap();
    txn_data->asset_op[index] = AssetOp::modify;
    return &txn_data->asset_metadata[index];
}

bool is_valid_txn(AssetTxn* txn)
{
    return txn->data()->handle != nullptr;
}

bool is_read_only(AssetTxn* txn)
{
    return txn->data()->access == AssetTxnAccess::read_only;
}

Result<const AssetMetadata*, AssetDatabaseError> read_serialized_asset(AssetTxn* txn, Serializer* serializer)
{
    auto* txn_data = txn->data();

    if (txn_data->asset_count >= BEE_ASSET_TXN_MAX_ASSETS)
    {
        return make_error(AssetDatabaseStatus::txn_max_asset_ops);
    }

    const int index = txn_data->asset_count;
    ++txn_data->asset_count;

    serialize(SerializerMode::reading, serializer, &txn_data->asset_metadata[index], txn_data->allocator);

    if (!asset_txn_get(txn_data, DbMapId::guid_to_asset, txn_data->asset_metadata->guid, &txn_data->asset_metadata[index]))
    {
        return make_error(AssetDatabaseStatus::not_found);
    }

    return &txn_data->asset_metadata[index];
}

Result<AssetMetadata*, AssetDatabaseError> modify_serialized_asset(AssetTxn* txn, Serializer* serializer)
{
    auto* txn_data = txn->data();

    if (txn_data->asset_count >= BEE_ASSET_TXN_MAX_ASSETS)
    {
        return make_error(AssetDatabaseStatus::txn_max_asset_ops);
    }

    const int index = txn_data->asset_count;
    ++txn_data->asset_count;

    serialize(SerializerMode::reading, serializer, &txn_data->asset_metadata[index], txn_data->allocator);

    auto props = BEE_MOVE(txn_data->asset_metadata->properties);

    if (!asset_txn_get(txn_data, DbMapId::guid_to_asset, txn_data->asset_metadata->guid, &txn_data->asset_metadata[index]))
    {
        return make_error(AssetDatabaseStatus::not_found);
    }

    txn_data->asset_metadata->properties = BEE_MOVE(props);
    return &txn_data->asset_metadata[index];
}

u128 get_artifact_hash(const void* buffer, const size_t buffer_size)
{
    return get_hash128(buffer, buffer_size, 0x284fa80);
}

Result<u128, AssetDatabaseError> add_artifact(AssetTxn* txn, const GUID guid, const Type artifact_type, const void* buffer, const size_t buffer_size)
{
    auto* txn_data = txn->data();

    if (!asset_exists(txn, guid))
    {
        return make_error(AssetDatabaseStatus::not_found);
    }

    const u128 hash = get_artifact_hash(buffer, buffer_size);
    TempAllocScope tmp_alloc(txn_data->db);
    Path artifact_path(tmp_alloc);
    get_artifact_path(txn, hash, &artifact_path);

    if (!asset_txn_put(txn_data, guid, hash, artifact_type->hash, 0))
    {
        return make_error(AssetDatabaseStatus::internal_error);
    }

    if (!artifact_path.exists())
    {
        auto artifact_dir = artifact_path.parent_path(tmp_alloc);
        if (!artifact_dir.exists())
        {
            fs::mkdir(artifact_dir, true);
        }

        if (!fs::write(artifact_path, buffer, buffer_size))
        {
            return make_error(AssetDatabaseStatus::failed_to_write_artifact_to_disk);
        }
    }

    return hash;
}

bool remove_artifact(AssetTxn* txn, const GUID guid, const u128& hash)
{
    auto* txn_data = txn->data();
    int remaining_artifacts = -1;
    if (!asset_txn_del(txn_data, guid, hash, &remaining_artifacts))
    {
        return false;
    }

    if (remaining_artifacts <= 0)
    {
        return true;
    }

    // delete the artifact from disk if no more guids reference it
    TempAllocScope tmp_alloc(txn_data->db);
    Path artifact_path(tmp_alloc);
    get_artifact_path(txn, hash, &artifact_path);
    return fs::remove(artifact_path);
}

Result<i32, AssetDatabaseError> get_artifacts(AssetTxn* txn, const GUID guid, AssetArtifact* dst)
{
    LMDBCursor cursor(txn->data(), db_get_dbi(txn->data()->db, DbMapId::guid_to_artifacts));
    if (!cursor)
    {
        return 0;
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

    if (cursor.count() == 1)
    {
        // MDB_NEXT_MULTIPLE won't work if we've only got one result
        memcpy(&dst[0], val.mv_data, val.mv_size);
        return 1;
    }

    int count = 0;
    AssetArtifact* multiple = dst;

    while (cursor.get(&guid_key, &val, MDB_NEXT_MULTIPLE))
    {
        BEE_ASSERT(val.mv_size % sizeof(AssetArtifact) == 0);

        memcpy(multiple + count, val.mv_data, val.mv_size);
        count += static_cast<i32>(val.mv_size / sizeof(AssetArtifact));
    }

    return count;
}

bool add_dependency(AssetTxn* txn, const GUID parent, const GUID child)
{
    auto* txn_data = txn->data();
    auto mdb_key = make_key(parent);
    auto mdb_val = make_key(child);
    return basic_txn_put(txn_data->handle, db_get_dbi(txn_data->db, DbMapId::guid_to_dependencies), &mdb_key, &mdb_val, 0);
}

bool remove_dependency(AssetTxn* txn, const GUID parent, const GUID child)
{
    auto* txn_data = txn->data();
    auto mdb_key = make_key(parent);
    auto mdb_val = make_key(child);
    return basic_txn_del(txn_data->handle, db_get_dbi(txn_data->db, DbMapId::guid_to_dependencies), &mdb_key, &mdb_val);
}


} // namespace bee


BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    g_module.open = bee::open;
    g_module.close = bee::close;
    g_module.is_open = bee::is_open;
    g_module.location = bee::location;
    g_module.gc = bee::gc;
    g_module.read = bee::read;
    g_module.write = bee::write;
    g_module.abort = bee::abort;
    g_module.commit = bee::commit;
    g_module.is_valid_txn = bee::is_valid_txn;
    g_module.is_read_only = bee::is_read_only;
    g_module.asset_exists = bee::asset_exists;
    g_module.create_asset = bee::create_asset;
    g_module.delete_asset = bee::delete_asset;
    g_module.read_asset = bee::read_asset;
    g_module.modify_asset = bee::modify_asset;
    g_module.read_serialized_asset = bee::read_serialized_asset;
    g_module.modify_serialized_asset = bee::modify_serialized_asset;
    g_module.get_artifact_hash = bee::get_artifact_hash;
    g_module.add_artifact = bee::add_artifact;
    g_module.remove_artifact = bee::remove_artifact;
    g_module.add_dependency = bee::add_dependency;
    g_module.remove_dependency = bee::remove_dependency;
    g_module.get_artifacts = bee::get_artifacts;
    g_module.get_artifact_path = bee::get_artifact_path;

    loader->set_module(BEE_ASSET_DATABASE_MODULE_NAME, &g_module, state);
}

BEE_PLUGIN_VERSION(0, 0, 0)