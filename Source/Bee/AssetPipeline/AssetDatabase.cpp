/*
 *  AssetDatabase.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipeline/AssetDatabase.inl"

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Serialization/BinarySerializer.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"


#include <lmdb.h>


namespace bee {


AssetDatabaseModule g_assetdb{};

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
    { "GUIDToAsset",        MDB_CREATE },                                                       // guid_to_asset
    { "GUIDToProperties",   MDB_CREATE },                                                       // guid_to_properties
    { "GUIDToDependencies", MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, lmdb_compare_guid },       // guid_to_dependencies
    { "GUIDToArtifacts",    MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, lmdb_compare_artifact },   // guid_to_artifact
    { "GUIDToSubAssets",    MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, lmdb_compare_guid },       // guid_to_sub_assets
    { "SubAssetToOwner",    MDB_CREATE },                                                       // sub_asset_to_owner
    { "ArtifactToGUID",     MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED, lmdb_compare_guid },       // artifact_to_guid
    { "PathToGUID",         MDB_CREATE },                                                       // path_to_guid
    { "GUIDToPath",         MDB_CREATE },                                                       // guid_to_path
    { "NameToGUID",         MDB_CREATE },                                                       // name_to_guid
    { "GUIDToName",         MDB_CREATE },                                                       // guid_to_name
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

    template <typename T>
    i32 get_all(const GUID& guid, T* dst)
    {
        auto guid_key = make_key(guid);
        MDB_val val{};

        // Start at the first value in the GUID key
        if (!get(&guid_key, &val, MDB_SET_KEY))
        {
            return 0;
        }

        if (dst == nullptr)
        {
            return count();
        }

        if (count() == 1)
        {
            // MDB_NEXT_MULTIPLE won't work if we've only got one result
            memcpy(&dst[0], val.mv_data, val.mv_size);
            return 1;
        }

        int count = 0;
        T* multiple = dst;

        while (get(&guid_key, &val, MDB_NEXT_MULTIPLE))
        {
            BEE_ASSERT(val.mv_size % sizeof(T) == 0);

            memcpy(multiple + count, val.mv_data, val.mv_size);
            count += static_cast<i32>(val.mv_size / sizeof(T));
        }

        return count;
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

static void db_thread_init(AssetDatabase::ThreadData* thread)
{
    new (&thread->txn_allocator) ChunkAllocator(megabytes(2), 64, 0);
    new (&thread->tmp_allocator) LinearAllocator(megabytes(8), system_allocator());
}

static AssetDatabase::ThreadData& db_get_thread(AssetDatabase* db)
{
    return db->thread_data[job_worker_id()];
}

static unsigned int db_get_dbi(AssetDatabase* db, const DbMapId id)
{
    return db->db_maps[static_cast<int>(id)];
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

AssetDatabase* open(const PathView& location)
{
    const auto dir = location.parent();

    if (BEE_FAIL_F(dir.exists(), "Cannot open AssetDB: directory \"%" BEE_PRIsv "\" does not exist", BEE_FMT_SV(dir)))
    {
        return nullptr;
    }

    auto* db = BEE_NEW(system_allocator(), AssetDatabase);
    db->location.append(location);
    db->artifacts_root.append(location.parent()).append("Artifacts");
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

    if (BEE_LMDB_FAIL(mdb_env_set_maxdbs(db->env, static_cast<int>(DbMapId::count))))
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

PathView location(AssetDatabase* db)
{
    return db->location.view();
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
    return { &g_assetdb, db_create_txn(db, AssetTxnAccess::read_only) };
}

AssetTxn write(AssetDatabase* db)
{
    return { &g_assetdb, db_create_txn(db, AssetTxnAccess::read_write) };
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

    BEE_LMDB_ASSERT(mdb_txn_commit(txn_data->handle));
    txn_data->handle = nullptr;

    db_txn_list_remove(txn_data);
    db_txn_list_append(&txn_data->db->thread_data[txn_data->thread].gc_transactions, txn_data);

    return true;
}

bool asset_exists(AssetTxn* txn, const GUID guid)
{
    BEE_ASSERT(asset_txn_is_valid(txn->data()));
    auto mdb_key = make_key(guid);
    MDB_val mdb_val{};
    return basic_txn_get(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::guid_to_asset), &mdb_key, &mdb_val);
}

Result<AssetInfo*, AssetDatabaseError> create_asset(AssetTxn* txn)
{
    auto* txn_data = txn->data();
    auto* txn_handle = txn_data->handle;
    auto* db = txn_data->db;

    const auto guid = generate_guid();
    auto key = make_key(guid);
    MDB_val val{};
    val.mv_data = nullptr;
    val.mv_size = sizeof(AssetInfo);

    if (!basic_txn_put(txn_handle, db_get_dbi(db, DbMapId::guid_to_asset), &key, &val, MDB_RESERVE))
    {
        return { AssetDatabaseError::lmdb_error };
    }

    auto* meta = static_cast<AssetInfo*>(val.mv_data);
    meta->guid = guid;
    return meta;
}

Result<void, AssetDatabaseError> remove_all_artifacts(AssetTxn* txn, const GUID guid);
Result<void, AssetDatabaseError> remove_all_dependencies(AssetTxn* txn, const GUID guid);
Result<void, AssetDatabaseError> remove_all_sub_assets(AssetTxn* txn, const GUID owner);

Result<void, AssetDatabaseError> delete_asset(AssetTxn* txn, const GUID guid)
{
    if (!asset_exists(txn, guid))
    {
        return { AssetDatabaseError::not_found };
    }

    auto guid_key = make_key(guid);
    auto* db = txn->data()->db;
    auto* handle = txn->data()->handle;

    // Delete asset name mappings
    MDB_val name{};
    if (basic_txn_get(handle, db_get_dbi(db, DbMapId::guid_to_name), &guid_key, &name))
    {
        if (!basic_txn_del(handle, db_get_dbi(db, DbMapId::name_to_guid), &name, nullptr))
        {
            return { AssetDatabaseError::lmdb_error };
        }

        if (!basic_txn_del(handle, db_get_dbi(db, DbMapId::guid_to_name), &guid_key, nullptr))
        {
            return { AssetDatabaseError::lmdb_error };
        }
    }

    // Delete asset path mappings
    MDB_val path{};
    if (basic_txn_get(handle, db_get_dbi(db, DbMapId::guid_to_path), &guid_key, &path))
    {
        if (!basic_txn_del(handle, db_get_dbi(db, DbMapId::path_to_guid), &path, nullptr))
        {
            return { AssetDatabaseError::lmdb_error };
        }

        if (!basic_txn_del(handle, db_get_dbi(db, DbMapId::guid_to_path), &guid_key, nullptr))
        {
            return { AssetDatabaseError::lmdb_error };
        }
    }

    // Delete all the artifact mappings
    auto res = remove_all_artifacts(txn, guid);
    if (!res)
    {
        return res.unwrap_error();
    }

    // Delete all dependency mappings
    res = remove_all_dependencies(txn, guid);
    if (!res)
    {
        return res.unwrap_error();
    }

    // Delete all the sub-assets
    res = remove_all_sub_assets(txn, guid);
    if (!res)
    {
        return res.unwrap_error();
    }

    // Delete the owner->sub-asset mapping for this assets parent GUID if it has one
    MDB_val owner_key{};
    if (basic_txn_get(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::sub_asset_to_owner), &guid_key, &owner_key))
    {
        if (!basic_txn_del(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::guid_to_sub_assets), &owner_key, &guid_key))
        {
            return { AssetDatabaseError::lmdb_error };
        }
        if (!basic_txn_del(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::sub_asset_to_owner), &guid_key, nullptr))
        {
            return { AssetDatabaseError::lmdb_error };
        }
    }

    BEE_ASSERT(asset_txn_is_valid(txn->data()));

    if (!basic_txn_del(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::guid_to_asset), &guid_key, nullptr))
    {
        return { AssetDatabaseError::lmdb_error };
    }

    return {};
}

Result<AssetInfo, AssetDatabaseError> get_asset_info(AssetTxn* txn, const GUID guid)
{
    auto key = make_key(guid);
    MDB_val val{};

    if (!basic_txn_get(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::guid_to_asset), &key, &val))
    {
        return { AssetDatabaseError::not_found };
    }

    BEE_ASSERT(val.mv_size == sizeof(AssetInfo));
    return *static_cast<AssetInfo*>(val.mv_data);
}

Result<void, AssetDatabaseError> set_asset_info(AssetTxn* txn, const AssetInfo& info)
{
    auto key = make_key(info.guid);
    MDB_val val{};
    val.mv_size = sizeof(AssetInfo);
    val.mv_data = const_cast<void*>(static_cast<const void*>(&info));

    if (!basic_txn_put(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::guid_to_asset), &key, &val, 0))
    {
        return { AssetDatabaseError::lmdb_error };
    }

    return {};
}

Result<void, AssetDatabaseError> set_import_settings(AssetTxn* txn, const GUID guid, const TypeInstance& settings)
{
    if (!asset_exists(txn, guid))
    {
        return { AssetDatabaseError::not_found };
    }

    auto& serialized_settings = db_get_thread(txn->data()->db).serialization_buffer;
    serialized_settings.clear();
    BinarySerializer serializer(&serialized_settings);
    serialize(SerializerMode::writing, &serializer, const_cast<TypeInstance*>(&settings), temp_allocator());

    auto key = make_key(guid);
    MDB_val val{};
    val.mv_size = serialized_settings.size();
    val.mv_data = serialized_settings.data();

    if (!basic_txn_put(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::guid_to_properties), &key, &val, 0))
    {
        return { AssetDatabaseError::lmdb_error };
    }

    return {};
}

Result<TypeInstance, AssetDatabaseError> get_import_settings(AssetTxn* txn, const GUID guid, Allocator* allocator)
{
    auto key = make_key(guid);
    MDB_val val{};

    if (!basic_txn_get(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::guid_to_properties), &key, &val))
    {
        return { AssetDatabaseError::not_found };
    }

    TypeInstance settings;
    io::MemoryStream stream(val.mv_data, val.mv_size);
    StreamSerializer serializer(&stream);
    serialize(SerializerMode::reading, &serializer, &settings, allocator);
    return BEE_MOVE(settings);
}

Result<void, AssetDatabaseError> set_asset_path(AssetTxn* txn, const GUID guid, const StringView path)
{
    if (!asset_exists(txn, guid))
    {
        return { AssetDatabaseError::not_found };
    }

    auto key = make_key(guid);
    MDB_val val{};
    val.mv_size = path.size();
    val.mv_data = static_cast<void*>(const_cast<char*>(path.data()));

    if (!basic_txn_put(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::path_to_guid), &val, &key, 0))
    {
        return { AssetDatabaseError::lmdb_error };
    }

    if (!basic_txn_put(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::guid_to_path), &key, &val, 0))
    {
        return { AssetDatabaseError::lmdb_error };
    }

    return {};
}

Result<StringView, AssetDatabaseError> get_asset_path(AssetTxn* txn, const GUID guid)
{
    auto key = make_key(guid);
    MDB_val val{};
    if (!basic_txn_get(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::guid_to_path), &key, &val))
    {
        return { AssetDatabaseError::not_found };
    }

    return StringView(static_cast<const char*>(val.mv_data), val.mv_size);
}

Result<GUID, AssetDatabaseError> get_guid_from_path(AssetTxn* txn, const StringView path)
{
    auto key = make_key(path);
    MDB_val val{};
    if (!basic_txn_get(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::path_to_guid), &key, &val))
    {
        return { AssetDatabaseError::not_found };
    }

    BEE_ASSERT(val.mv_size == sizeof(GUID));
    return *static_cast<GUID*>(val.mv_data);
}

Result<void, AssetDatabaseError> set_asset_name(AssetTxn* txn, const GUID guid, const AssetName& name)
{
    if (!asset_exists(txn, guid))
    {
        return { AssetDatabaseError::not_found };
    }

    auto key = make_key(guid);

    MDB_val val{};
    val.mv_size = name.size;
    val.mv_data = const_cast<void*>(name.data);

    if (!basic_txn_put(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::name_to_guid), &val, &key, 0))
    {
        return { AssetDatabaseError::lmdb_error };
    }

    if (!basic_txn_put(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::guid_to_name), &key, &val, 0))
    {
        return { AssetDatabaseError::lmdb_error };
    }

    return {};
}

Result<AssetName, AssetDatabaseError> get_asset_name(AssetTxn* txn, const GUID guid)
{
    auto key = make_key(guid);
    MDB_val val{};
    if (!basic_txn_get(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::guid_to_name), &key, &val))
    {
        return { AssetDatabaseError::not_found };
    }

    AssetName blob{};
    blob.size = val.mv_size;
    blob.data = val.mv_data;
    return blob;
}

Result<GUID, AssetDatabaseError> get_guid_from_name(AssetTxn* txn, const AssetName& name)
{
    MDB_val key{};
    key.mv_size = name.size;
    key.mv_data = const_cast<void*>(name.data);
    MDB_val val{};
    if (!basic_txn_get(txn->data()->handle, db_get_dbi(txn->data()->db, DbMapId::name_to_guid), &key, &val))
    {
        return { AssetDatabaseError::not_found };
    }

    BEE_ASSERT(val.mv_size == sizeof(GUID));
    return *static_cast<GUID*>(val.mv_data);
}

bool is_valid_txn(AssetTxn* txn)
{
    return txn->data()->handle != nullptr;
}

bool is_read_only(AssetTxn* txn)
{
    return txn->data()->access == AssetTxnAccess::read_only;
}

u128 get_artifact_hash(const void* buffer, const size_t buffer_size)
{
    return get_hash128(buffer, buffer_size, 0x284fa80);
}

Result<u128, AssetDatabaseError> add_artifact_with_key(AssetTxn* txn, const GUID guid, const Type artifact_type, const u32 artifact_key, const void* buffer, const size_t buffer_size)
{
    auto* txn_data = txn->data();

    if (!asset_exists(txn, guid))
    {
        return { AssetDatabaseError::not_found };
    }

    const u128 hash = get_artifact_hash(buffer, buffer_size);
    TempAllocScope tmp_alloc(txn_data->db);
    Path artifact_path(tmp_alloc);
    get_artifact_path(txn, hash, &artifact_path);

    BEE_ASSERT(asset_txn_is_valid(txn_data));

    AssetArtifact artifact{};
    artifact.type_hash = artifact_type->hash;
    artifact.content_hash = hash;
    artifact.key = artifact_key;

    auto guid_key = make_key(guid);
    MDB_val mdb_val{};
    mdb_val.mv_data = &artifact;
    mdb_val.mv_size = sizeof(AssetArtifact);

    // Put the guid->artifact mapping in
    if (!basic_txn_put(txn_data->handle, db_get_dbi(txn_data->db, DbMapId::guid_to_artifacts), &guid_key, &mdb_val, 0))
    {
        return { AssetDatabaseError::lmdb_error };
    }

    // Map back from hash->guid
    auto hash_key = make_key(hash);
    if (!basic_txn_put(txn_data->handle, db_get_dbi(txn_data->db, DbMapId::artifact_to_guid), &hash_key, &guid_key, 0))
    {
        return { AssetDatabaseError::lmdb_error };;
    }

    if (!artifact_path.exists())
    {
        auto artifact_dir = artifact_path.parent();
        if (!artifact_dir.exists())
        {
            fs::mkdir(artifact_dir, true);
        }

        auto file = fs::open_file(artifact_path.view(), fs::OpenMode::write);
        if (!fs::write(file, buffer, buffer_size))
        {
            return { AssetDatabaseError::failed_to_write_artifact_to_disk };
        }
    }

    return hash;
}

Result<u128, AssetDatabaseError> add_artifact(AssetTxn* txn, const GUID guid, const Type artifact_type, const void* buffer, const size_t buffer_size)
{
    return add_artifact_with_key(txn, guid, artifact_type, 0, buffer, buffer_size);
}

Result<void, AssetDatabaseError> remove_artifact(AssetTxn* txn, const GUID guid, const u128& hash)
{
    auto* txn_data = txn->data();
    int remaining_artifacts = -1;
    auto mdb_guid = make_key(guid);
    auto mdb_hash = make_key(hash);

    {
        // Find the hash->GUID mapping and delete it
        LMDBCursor cursor(txn_data, db_get_dbi(txn_data->db, DbMapId::artifact_to_guid));
        if (!cursor || !cursor.get(&mdb_hash, &mdb_guid, MDB_GET_BOTH))
        {
            return { AssetDatabaseError::not_found };
        }

        // delete the disk asset if nothing is referencing this artifact
        remaining_artifacts = cursor.count() - 1;

        if (!cursor.del())
        {
            return { AssetDatabaseError::lmdb_error };
        }
    }

    // Delete the GUID->artifact mapping
    if (!basic_txn_del(txn_data->handle, db_get_dbi(txn_data->db, DbMapId::guid_to_artifacts), &mdb_guid, &mdb_hash))
    {
        return { AssetDatabaseError::lmdb_error };
    }

    if (remaining_artifacts > 0)
    {
        return {};
    }

    // delete the artifact from disk if no more guids reference it
    TempAllocScope tmp_alloc(txn_data->db);
    Path artifact_path(tmp_alloc);
    get_artifact_path(txn, hash, &artifact_path);
    if (!fs::remove(artifact_path.view()))
    {
        return { AssetDatabaseError::failed_to_write_artifact_to_disk };
    }

    // Remove the directory if this is the last artifact file remaining
    bool is_empty_artifact_dir = true;
    for (const auto path : fs::read_dir(artifact_path.parent()))
    {
        if (fs::is_file(path))
        {
            is_empty_artifact_dir = false;
            break;
        }
    }

    if (is_empty_artifact_dir)
    {
        fs::rmdir(artifact_path.parent(), true);
    }

    return {};
}

Result<i32, AssetDatabaseError> get_artifacts(AssetTxn* txn, const GUID guid, AssetArtifact* dst)
{
    LMDBCursor cursor(txn->data(), db_get_dbi(txn->data()->db, DbMapId::guid_to_artifacts));
    if (!cursor)
    {
        return 0;
    }

    return cursor.get_all(guid, dst);
}

Result<AssetArtifact, AssetDatabaseError> find_artifact(AssetTxn* txn, const GUID guid, const u32 artifact_key)
{
    auto res = get_artifacts(txn, guid, nullptr);
    if (!res)
    {
        return res.unwrap_error();
    }

    const int count = res.unwrap();
    auto* artifacts = BEE_ALLOCA_ARRAY(AssetArtifact, count);
    res = get_artifacts(txn, guid, artifacts);
    if (!res)
    {
        return res.unwrap_error();
    }

    const int artifact_index = find_index_if(artifacts, artifacts + count, [&](const AssetArtifact& artifact)
    {
        return artifact.key == artifact_key;
    });

    if (artifact_index < 0)
    {
        return { AssetDatabaseError::not_found };
    }

    return artifacts[artifact_index];
}

Result<void, AssetDatabaseError> remove_all_artifacts(AssetTxn* txn, const GUID guid)
{
    auto res = get_artifacts(txn, guid, nullptr);
    if (!res)
    {
        return res.unwrap_error();
    }

    const int count = res.unwrap();
    auto* artifacts = BEE_ALLOCA_ARRAY(AssetArtifact, count);
    res = get_artifacts(txn, guid, artifacts);
    if (!res)
    {
        return res.unwrap_error();
    }

    for (int i = 0; i < count; ++i)
    {
        auto remove_res = remove_artifact(txn, guid, artifacts[i].content_hash);
        if (!remove_res)
        {
            return remove_res.unwrap_error();
        }
    }

    return {};
}

Result<void, AssetDatabaseError> add_dependency(AssetTxn* txn, const GUID guid, const GUID dependency)
{
    auto* txn_data = txn->data();
    auto mdb_key = make_key(guid);
    auto mdb_val = make_key(dependency);
    if (!basic_txn_put(txn_data->handle, db_get_dbi(txn_data->db, DbMapId::guid_to_dependencies), &mdb_key, &mdb_val, 0))
    {
        return { AssetDatabaseError::lmdb_error };
    }
    return {};
}

Result<void, AssetDatabaseError> remove_dependency(AssetTxn* txn, const GUID guid, const GUID dependency)
{
    auto* txn_data = txn->data();
    auto mdb_key = make_key(guid);
    auto mdb_val = make_key(dependency);
    if (!basic_txn_del(txn_data->handle, db_get_dbi(txn_data->db, DbMapId::guid_to_dependencies), &mdb_key, &mdb_val))
    {
        return { AssetDatabaseError::lmdb_error };
    }
    return {};
}

Result<void, AssetDatabaseError> remove_all_dependencies(AssetTxn* txn, const GUID guid)
{
    auto* txn_data = txn->data();
    auto mdb_key = make_key(guid);
    MDB_val val{};

    if (!basic_txn_get(txn_data->handle, db_get_dbi(txn_data->db, DbMapId::guid_to_dependencies), &mdb_key, &val))
    {
        return {}; // no dependencies
    }

    if (!basic_txn_del(txn_data->handle, db_get_dbi(txn_data->db, DbMapId::guid_to_dependencies), &mdb_key, nullptr))
    {
        return { AssetDatabaseError::lmdb_error };
    }
    return {};
}

Result<void, AssetDatabaseError> set_sub_asset(AssetTxn* txn, const GUID owner, const GUID sub_asset)
{
    auto* txn_data = txn->data();
    auto mdb_key = make_key(owner);
    auto mdb_val = make_key(sub_asset);
    if (!basic_txn_put(txn_data->handle, db_get_dbi(txn_data->db, DbMapId::guid_to_sub_assets), &mdb_key, &mdb_val, 0))
    {
        return { AssetDatabaseError::lmdb_error };
    }
    if (!basic_txn_put(txn_data->handle, db_get_dbi(txn_data->db, DbMapId::sub_asset_to_owner), &mdb_val, &mdb_key, 0))
    {
        return { AssetDatabaseError::lmdb_error };
    }
    return {};
}

Result<i32, AssetDatabaseError> get_sub_assets(AssetTxn* txn, const GUID guid, GUID* dst)
{
    LMDBCursor cursor(txn->data(), db_get_dbi(txn->data()->db, DbMapId::guid_to_sub_assets));
    if (!cursor)
    {
        return 0;
    }

    return cursor.get_all(guid, dst);
}

Result<void, AssetDatabaseError> remove_all_sub_assets(AssetTxn* txn, const GUID owner)
{
    auto owner_key = make_key(owner);

    LMDBCursor cursor(txn->data(), db_get_dbi(txn->data()->db, DbMapId::guid_to_sub_assets));
    if (!cursor)
    {
        return { AssetDatabaseError::lmdb_error };
    }

    const int count = cursor.get_all<GUID>(owner, nullptr);
    if (count <= 0)
    {
        return {}; // no sub assets
    }

    auto* sub_assets = BEE_ALLOCA_ARRAY(GUID, count);
    cursor.get_all<GUID>(owner, sub_assets);
    cursor.close();

    for (int i = 0; i < count; ++i)
    {
        auto res = delete_asset(txn, sub_assets[i]);
        if (!res)
        {
            return res.unwrap_error();
        }
    }

    return {};
}

void set_asset_database_module(bee::PluginLoader* loader, const bee::PluginState state)
{
    g_assetdb.open = bee::open;
    g_assetdb.close = bee::close;
    g_assetdb.is_open = bee::is_open;
    g_assetdb.location = bee::location;
    g_assetdb.gc = bee::gc;
    g_assetdb.read = bee::read;
    g_assetdb.write = bee::write;
    g_assetdb.abort = bee::abort;
    g_assetdb.commit = bee::commit;
    g_assetdb.is_valid_txn = bee::is_valid_txn;
    g_assetdb.is_read_only = bee::is_read_only;
    g_assetdb.asset_exists = bee::asset_exists;

    g_assetdb.create_asset = bee::create_asset;
    g_assetdb.delete_asset = bee::delete_asset;
    g_assetdb.get_asset_info = bee::get_asset_info;
    g_assetdb.set_asset_info = bee::set_asset_info;

    g_assetdb.set_import_settings = bee::set_import_settings;
    g_assetdb.get_import_settings = bee::get_import_settings;

    g_assetdb.set_asset_path = bee::set_asset_path;
    g_assetdb.get_asset_path = bee::get_asset_path;
    g_assetdb.get_guid_from_path = bee::get_guid_from_path;

    g_assetdb.set_asset_name = bee::set_asset_name;
    g_assetdb.get_asset_name = bee::get_asset_name;
    g_assetdb.get_guid_from_name = bee::get_guid_from_name;

    g_assetdb.get_artifact_hash = bee::get_artifact_hash;
    g_assetdb.get_artifact_path = bee::get_artifact_path;
    g_assetdb.add_artifact = bee::add_artifact;
    g_assetdb.add_artifact_with_key = bee::add_artifact_with_key;
    g_assetdb.remove_artifact = bee::remove_artifact;
    g_assetdb.remove_all_artifacts = bee::remove_all_artifacts;
    g_assetdb.get_artifacts = bee::get_artifacts;

    g_assetdb.add_dependency = bee::add_dependency;
    g_assetdb.remove_dependency = bee::remove_dependency;
    g_assetdb.remove_all_dependencies = bee::remove_all_dependencies;

    g_assetdb.set_sub_asset = bee::set_sub_asset;
    g_assetdb.remove_all_sub_assets = bee::remove_all_sub_assets;
    g_assetdb.get_sub_assets = bee::get_sub_assets;

    loader->set_module(BEE_ASSET_DATABASE_MODULE_NAME, &g_assetdb, state);
}


} // namespace bee