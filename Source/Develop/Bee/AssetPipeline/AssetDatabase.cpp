/*
 *  AssetDatabase.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipeline/AssetDatabase.hpp"
#include "Bee/Core/Memory/Memory.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"

#include <lmdb.h>

namespace bee {


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

AssetDB::~AssetDB()
{
    close();
}

bool AssetDB::open(const char* assets_root, const char* location, const char* name)
{
    assets_root_ = assets_root;
    path_ = Path(location);

    if (!path_.exists())
    {
        const auto mkdir_success = fs::mkdir(path_);
        BEE_ASSERT(mkdir_success);
    }

    path_.append(name);
    artifacts_root_ = location;

    BEE_ASSERT(env_ == nullptr);
    if (BEE_LMDB_FAIL(mdb_env_create(&env_)))
    {
        close();
        return false;
    }

    // Setup assertions and max DBI's for environment - MUST BE CONFIGURED PRIOR TO `mdb_env_open`
    const auto result = mdb_env_set_assert(env_, &lmdb_assert_callback);
    BEE_LMDB_ASSERT(result);

    if (BEE_LMDB_FAIL(mdb_env_set_maxdbs(env_, 2)))
    {
        close();
        return false;
    }

    /*
     * - Default flags
     * - unix permissions (ignored on windows): -rw-rw-r--
     * - NOSUBDIR - custom database filename
     */
    if (BEE_LMDB_FAIL(mdb_env_open(env_, path_.c_str(), MDB_NOSUBDIR, 0664)))
    {
        close();
        return false;
    }

    MDB_txn* txn = nullptr;
    if (BEE_LMDB_FAIL(mdb_txn_begin(env_, nullptr, 0, &txn)))
    {
        return false;
    }

    // Open handles to both databases - name map and asset storage
    const auto asset_dbi_result = mdb_dbi_open(txn, asset_dbi_name_, MDB_CREATE, &asset_dbi_);
    const auto name_dbi_result = mdb_dbi_open(txn, name_dbi_name_, MDB_CREATE, &name_dbi_);

    if (BEE_LMDB_FAIL(asset_dbi_result) || BEE_LMDB_FAIL(name_dbi_result))
    {
        mdb_txn_abort(txn);
        close();
        return false;
    }

    if (BEE_LMDB_FAIL(mdb_txn_commit(txn)))
    {
        return false;
    }

    return true;
}

void AssetDB::close()
{
    if (env_ == nullptr)
    {
        return;
    }

    if (asset_dbi_ != invalid_dbi_)
    {
        mdb_dbi_close(env_, asset_dbi_);
    }

    if (name_dbi_ != invalid_dbi_)
    {
        mdb_dbi_close(env_, name_dbi_);
    }

    mdb_env_close(env_);
    env_ = nullptr;
}

bool AssetDB::is_valid()
{
    return env_ != nullptr && asset_dbi_ != invalid_dbi_ && name_dbi_ != invalid_dbi_;
}

AssetDB::Transaction::Transaction(MDB_env* env, const unsigned long flags)
{
    BEE_ASSERT(env != nullptr);

    const auto result = mdb_txn_begin(env, nullptr, flags, &ptr);
    BEE_LMDB_ASSERT(result);
}

AssetDB::Transaction::~Transaction()
{
    if (ptr != nullptr)
    {
        abort();
    }
}

void AssetDB::Transaction::commit()
{
    BEE_ASSERT(ptr != nullptr);

    mdb_txn_commit(ptr);
    ptr = nullptr;
}

void AssetDB::Transaction::abort()
{
    BEE_ASSERT(ptr != nullptr);
    mdb_txn_abort(ptr);
    ptr = nullptr;
}


MDB_val make_key(const GUID& guid)
{
    MDB_val val{};
    val.mv_size = sizeof(GUID);
    val.mv_data = const_cast<u8*>(guid.data);
    return val;
}


MDB_val make_key(const StringView& name)
{
    MDB_val val{};
    val.mv_size = sizeof(char) * name.size();
    val.mv_data = const_cast<char*>(name.data());
    return val;
}

// Temporary buffer for storing records
static thread_local char value_buffer[4096];

MDB_val make_value(const AssetMeta& meta, const char* path)
{
    const auto src_length = str::length(path);

    BEE_ASSERT(src_length > 0);

    io::MemoryStream stream(value_buffer, static_array_length(value_buffer), 0);
    stream.write(&meta, sizeof(AssetMeta));
    stream.write(path, src_length);

    MDB_val val{};
    val.mv_size = stream.size();
    val.mv_data = stream.data();
    return val;
}

void assetdb_record_serialize(const MDB_val& value, AssetMeta* meta, const char** path)
{
    const auto size = math::min(sign_cast<i32>(value.mv_size), static_array_length(value_buffer));
    memcpy(value_buffer, value.mv_data, size);
    if (meta != nullptr)
    {
        memcpy(meta, value_buffer, sizeof(AssetMeta));
    }

    const auto path_ptr = &value_buffer[0] + sizeof(AssetMeta);

    if (path != nullptr)
    {
        *path = path_ptr;
    }
}

bool AssetDB::asset_exists(const GUID& guid)
{
    BEE_ASSERT(is_valid());

    MDB_val val{};
    Transaction txn(env_, MDB_RDONLY);
    auto key = make_key(guid);

    const auto result = mdb_get(*txn, asset_dbi_, &key, &val);

    txn.commit();

    return result == MDB_SUCCESS;
}

bool AssetDB::put_asset(const GUID& guid, const char* src_path)
{
    BEE_ASSERT(is_valid());
    Transaction txn(env_);
    AssetMeta meta(guid, Type{}, "");
    return put_asset(txn, meta, src_path);
}

bool AssetDB::put_asset(const AssetMeta& meta, const char* src_path)
{
    BEE_ASSERT(is_valid());
    Transaction txn(env_);
    return put_asset(txn, meta, src_path);
}

bool AssetDB::put_asset(Transaction& txn, const AssetMeta& meta, const char* src_path)
{
    // Update the data in the asset dbi
    auto key = make_key(meta.guid);
    auto val = make_value(meta, src_path);

    if (BEE_LMDB_FAIL(mdb_put(*txn, asset_dbi_, &key, &val, 0)))
    {
        return false;
    }

    // If the meta has a valid name then update that too
    if (str::length(meta.name) > 0)
    {
        key = make_key(meta.name);
        val = make_key(meta.guid);

        if (BEE_LMDB_FAIL(mdb_put(*txn, name_dbi_, &key, &val, 0)))
        {
            return false;
        }
    }

    txn.commit();

    return true;
}

bool AssetDB::get_asset(const GUID& guid, AssetMeta* meta)
{
    BEE_ASSERT(is_valid());
    Transaction txn(env_, MDB_RDONLY);
    return get_asset(txn, guid, meta, nullptr);
}

bool AssetDB::get_asset(Transaction& txn, const GUID& guid, AssetMeta* meta, const char** src_path)
{
    if (BEE_FAIL(meta != nullptr))
    {
        return false;
    }

    MDB_val val{};
    auto key = make_key(guid);
    const auto result = mdb_get(*txn, asset_dbi_, &key, &val);

    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    // Get the actual meta data
    txn.commit();

    assetdb_record_serialize(val, meta, src_path);

    return true;
}

bool AssetDB::get_source_path(const GUID& guid, Path* path)
{
    BEE_ASSERT(is_valid());

    if (BEE_FAIL(path != nullptr))
    {
        return false;
    }

    Transaction txn(env_, MDB_RDONLY);
    MDB_val val{};
    auto key = make_key(guid);
    const auto result = mdb_get(*txn, asset_dbi_, &key, &val);

    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    txn.commit();

    const char* src_path_ptr = nullptr;
    assetdb_record_serialize(val, nullptr, &src_path_ptr);

    path->clear();
    path->append(assets_root_).append(src_path_ptr);
    return true;
}

bool AssetDB::get_artifact_path(const GUID& guid, Path* path)
{
    static thread_local char guid_string_buffer[33];

    BEE_ASSERT(is_valid());
    if (BEE_FAIL(path != nullptr))
    {
        return false;
    }

    // Use the existing meta buffer to create the path to look like e.g: "3c\\3cf924e1bb4d47779c6014e532b49357"
    guid_to_string(guid, GUIDFormat::digits, guid_string_buffer, static_array_length(guid_string_buffer));

    path->clear();
    path->append(artifacts_root_).append(StringView(guid_string_buffer, 2)).append(guid_string_buffer);

    return true;
}


bool AssetDB::delete_asset(const GUID& guid)
{
    BEE_ASSERT(is_valid());

    Transaction txn(env_);
    auto key = make_key(guid);

    set_asset_name(txn, guid, StringView{}); // erase the asset name before deleting

    if (BEE_LMDB_FAIL(mdb_del(*txn, asset_dbi_, &key, nullptr)))
    {
        return false;
    }

    txn.commit();
    return true;
}

bool AssetDB::get_asset_name(const GUID& guid, io::StringStream* dst)
{
    BEE_ASSERT(is_valid());
    BEE_ASSERT(dst != nullptr);

    AssetMeta meta{};
    if (!get_asset(guid, &meta))
    {
        return false;
    }

    dst->write(meta.name, static_array_length(meta.name));
    return true;
}

bool AssetDB::set_asset_name(const GUID& guid, const StringView& name)
{
    BEE_ASSERT(is_valid());
    Transaction txn(env_);
    return set_asset_name(txn, guid, name);
}

bool AssetDB::erase_asset_name(const GUID& guid)
{
    BEE_ASSERT(is_valid());

    Transaction txn(env_);
    return set_asset_name(txn, guid, StringView{});
}

bool AssetDB::set_asset_name(Transaction& txn, const GUID& guid, const StringView& name)
{
    // Update the name in the asset dbi
    const char* src_path = nullptr;
    AssetMeta meta{};
    if (!get_asset(txn, guid, &meta, &src_path))
    {
        return false;
    }

    auto name_key = make_key(meta.name);

    // Replace the existing name entry with a new one - empty names are just deleted
    const auto result = mdb_del(*txn, name_dbi_, &name_key, nullptr);

    if (result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result))
    {
        name_key = make_key(name);
        auto guid_key = make_key(guid);
        if (BEE_LMDB_FAIL(mdb_put(*txn, name_dbi_, &name_key, &guid_key, 0)))
        {
            return false;
        }
    }

    return put_asset(txn, meta, src_path);
}

bool AssetDB::get_asset_guid(const StringView& name, GUID* guid)
{
    BEE_ASSERT(is_valid());
    BEE_ASSERT(guid != nullptr);

    Transaction txn(env_, MDB_RDONLY);
    auto key = make_key(name);

    MDB_val val{};
    const auto result = mdb_get(*txn, name_dbi_, &key, &val);

    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    memcpy(guid->data, val.mv_data, math::min(val.mv_size, sign_cast<size_t>(static_array_length(guid->data))));
    return true;
}


} // namespace bee
