/*
 *  AssetDatabaseV2.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipeline/AssetDatabase.hpp"
#include "Bee/Core/Serialization/BinarySerializer.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"

#include <lmdb.h>

namespace bee {


static constexpr auto g_assets_dbi_name = "Assets";
static constexpr auto g_name_to_guid_dbi_name = "NameToGUID";
static constexpr auto g_guid_to_name_dbi_name = "GUIDToName";
static constexpr auto g_hash_to_artifact_dbi_name = "Artifacts";
static constexpr auto g_invalid_dbi = limits::max<u32>();
static constexpr auto g_max_dbs = 4;

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
 * Transaction - implementation
 *
 ********************************
 */
AssetDbTxn::AssetDbTxn(const AssetDbDescriptor& desc)
    : desc_(desc)
{
    if (desc.env == nullptr)
    {
        return;
    }

    MDB_txn* txn = nullptr;

    const auto flags = desc.kind == AssetDbTxnKind::read_only ? MDB_RDONLY : 0;
    if (BEE_LMDB_FAIL(mdb_txn_begin(desc.env, nullptr, flags, &txn)))
    {
        return;
    }

    ptr_ = txn;
}

AssetDbTxn::~AssetDbTxn()
{
    destroy();
}

AssetDbTxn::AssetDbTxn(bee::AssetDbTxn&& other) noexcept
{
    move_construct(other);
}

AssetDbTxn& AssetDbTxn::operator=(AssetDbTxn&& other) noexcept
{
    move_construct(other);
    return *this;
}

void AssetDbTxn::move_construct(bee::AssetDbTxn& other) noexcept
{
    destroy();

    desc_ = other.desc_;
    ptr_ = other.ptr_;

    new (&other.desc_) AssetDbDescriptor{};
    other.ptr_ = nullptr;
}

void AssetDbTxn::destroy()
{
    switch(desc_.kind)
    {
        case AssetDbTxnKind::read_write:
        {
            abort();
            break;
        }
        case AssetDbTxnKind::read_only:
        {
            commit();
            break;
        }
        default: break;
    }

    new (&desc_) AssetDbDescriptor{};
}

void AssetDbTxn::abort()
{
    if (desc_.kind == AssetDbTxnKind::invalid || ptr_ == nullptr)
    {
        return;
    }

    mdb_txn_abort(ptr_);
    destroy();
}

void AssetDbTxn::commit()
{
    if (desc_.kind == AssetDbTxnKind::invalid || ptr_ == nullptr)
    {
        return;
    }

    mdb_txn_commit(ptr_);
    destroy();
}

bool AssetDbTxn::put_asset(const AssetFile& asset)
{
    DynamicArray<u8> buffer(temp_allocator());
    BinarySerializer serializer(&buffer);
    serialize(SerializerMode::writing, &serializer, const_cast<AssetFile*>(&asset));

    auto guid_key = make_key(asset.guid);
    MDB_val val{};

    // Put the actual asset file data into the DB
    val.mv_size = buffer.size();
    val.mv_data = buffer.data();

    if(BEE_LMDB_FAIL(mdb_put(ptr_, desc_.assets_dbi, &guid_key, &val, MDB_RESERVE)))
    {
        return false;
    }

    return set_asset_name(asset.guid, asset.name.view());
}

bool AssetDbTxn::delete_asset(const GUID& guid)
{
    AssetFile asset(temp_allocator());
    if (!get_asset(guid, &asset))
    {
        return false;
    }

    // Delete all the assets artifacts
    for (auto& hash : asset.artifact_hashes)
    {
        if (!delete_artifact(hash))
        {
            return false;
        }
    }

    auto guid_key = make_key(guid);
    MDB_val val{};

    auto result = mdb_get(ptr_, desc_.guid_to_name_dbi, &guid_key, &val);
    if (BEE_LMDB_FAIL(result))
    {
        return false;
    }

    // Delete the GUID->Name & Name->GUID mappings if they exist
    if (result != MDB_NOTFOUND)
    {
        if (BEE_LMDB_FAIL(mdb_del(ptr_, desc_.name_to_guid_dbi, &val, nullptr)))
        {
            return false;
        }

        if (BEE_LMDB_FAIL(mdb_del(ptr_, desc_.guid_to_name_dbi, &val, nullptr)))
        {
            return false;
        }
    }

    result = mdb_del(ptr_, desc_.assets_dbi, &guid_key, nullptr);
    return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
}

bool AssetDbTxn::get_asset(const GUID& guid, AssetFile* asset)
{
    MDB_val val{};
    auto key = make_key(guid);

    const auto result = mdb_get(ptr_, desc_.assets_dbi, &key, &val);

    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    io::MemoryStream stream(val.mv_data, val.mv_size);
    StreamSerializer serializer(&stream);
    serialize(SerializerMode::reading, &serializer, asset);

    return true;
}

bool AssetDbTxn::has_asset(const GUID& guid)
{
    auto key = make_key(guid);
    MDB_val val{};
    const auto result = mdb_get(ptr_, desc_.assets_dbi, &key, &val);
    return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
}

bool AssetDbTxn::set_asset_name(const GUID& guid, const StringView& name)
{
    // TODO(JACOB): delete from the stored asset file as well
    if (!has_asset(guid))
    {
        log_error("No such asset %s", format_guid(guid, GUIDFormat::digits));
        return false;
    }

    auto guid_key = make_key(guid);
    MDB_val val{};

    const auto result = mdb_get(ptr_, desc_.guid_to_name_dbi, &guid_key, &val);
    if (BEE_LMDB_FAIL(result))
    {
        return false;
    }

    if (result != MDB_NOTFOUND && name != StringView(static_cast<const char*>(val.mv_data), val.mv_size))
    {
        // If the name has changed then we need to update the maps
        // Delete the old name
        if (BEE_LMDB_FAIL(mdb_del(ptr_, desc_.name_to_guid_dbi, &val, nullptr)))
        {
            return false;
        }
    }

    // delete the name entirely if the it's empty
    if (name.empty())
    {
        return !BEE_LMDB_FAIL(mdb_del(ptr_, desc_.guid_to_name_dbi, &guid_key, nullptr));
    }

    // Otherwise update the maps
    val = make_key(name);

    // Update the GUID->name DB with the new name
    if (BEE_LMDB_FAIL(mdb_put(ptr_, desc_.guid_to_name_dbi, &guid_key, &val, 0)))
    {
        return false;
    }

    // Update the Name->GUID DB with the new name
    if (BEE_LMDB_FAIL(mdb_put(ptr_, desc_.name_to_guid_dbi, &val, &guid_key, 0)))
    {
        return false;
    }

    return true;
}

bool AssetDbTxn::get_name_from_guid(const GUID& guid, String* name)
{
    auto key = make_key(guid);
    MDB_val val{};

    const auto result = mdb_get(ptr_, desc_.guid_to_name_dbi, &key, &val);
    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    name->append(StringView(static_cast<const char*>(val.mv_data), val.mv_size));
    return true;
}

bool AssetDbTxn::get_guid_from_name(const StringView& name, GUID* guid)
{
    auto key = make_key(name);
    MDB_val val{};

    const auto result = mdb_get(ptr_, desc_.name_to_guid_dbi, &key, &val);
    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    BEE_ASSERT(val.mv_size == static_array_length(guid->data));

    memcpy(guid->data, val.mv_data, val.mv_size);

    return true;
}

bool AssetDbTxn::put_artifact(const AssetArtifact& artifact)
{
    auto key = make_key(artifact.hash);
    MDB_val val{};

    // Put the actual asset file data into the DB
    val.mv_size = artifact.buffer_size;
    val.mv_data = const_cast<void*>(static_cast<const void*>(artifact.buffer));

    if(BEE_LMDB_FAIL(mdb_put(ptr_, desc_.artifact_dbi, &key, &val, MDB_RESERVE)))
    {
        return false;
    }

    return true;
}

bool AssetDbTxn::delete_artifact(const u128& hash)
{
    auto key = make_key(hash);
    const auto result = mdb_del(ptr_, desc_.artifact_dbi, &key, nullptr);
    return result != MDB_NOTFOUND && !BEE_LMDB_FAIL(result);
}

bool AssetDbTxn::get_artifact(const u128& hash, AssetArtifact* artifact)
{
    auto key = make_key(hash);
    MDB_val val{};

    const auto result = mdb_get(ptr_, desc_.guid_to_name_dbi, &key, &val);
    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        return false;
    }

    artifact->hash = hash;
    artifact->buffer_size = val.mv_size;
    artifact->buffer = static_cast<const u8*>(val.mv_data);
    return true;
}

bool AssetDbTxn::get_artifacts_from_guid(const GUID &guid, DynamicArray<AssetArtifact>* result)
{
    AssetFile asset(temp_allocator());
    if (!get_asset(guid, &asset))
    {
        return false;
    }

    AssetArtifact artifact{};

    for (auto& hash : asset.artifact_hashes)
    {
        if (!get_artifact(hash, &artifact))
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


/*
 ********************************
 *
 * Database - implementation
 *
 ********************************
 */
AssetDatabase::~AssetDatabase()
{
    if (desc_.env != nullptr)
    {
        close();
    }
}

void AssetDatabase::open(const Path& directory, const StringView& name)
{
    if (BEE_FAIL_F(desc_.env == nullptr, "AssetDB is already opened at path: %s", location_.c_str()))
    {
        return;
    }

    if (BEE_FAIL_F(directory.exists(), "Cannot open AssetDB: directory \"%s\" does not exist", directory.c_str()))
    {
        return;
    }

    location_ = directory.join(name);

    if (BEE_LMDB_FAIL(mdb_env_create(&desc_.env)))
    {
        close();
        return;
    }

    // Setup assertions and max DBI's for environment - MUST BE CONFIGURED PRIOR TO `mdb_env_open`
    const auto result = mdb_env_set_assert(desc_.env, &lmdb_assert_callback);
    BEE_LMDB_ASSERT(result);

    if (BEE_LMDB_FAIL(mdb_env_set_maxdbs(desc_.env, g_max_dbs)))
    {
        close();
        return;
    }

    /*
     * - Default flags
     * - unix permissions (ignored on windows): -rw-rw-r--
     * - NOSUBDIR - custom database filename
     */
    if (BEE_LMDB_FAIL(mdb_env_open(desc_.env, location_.c_str(), MDB_NOSUBDIR, 0664)))
    {
        close();
        return;
    }

    MDB_txn* txn = nullptr;
    if (BEE_LMDB_FAIL(mdb_txn_begin(desc_.env, nullptr, 0, &txn)))
    {
        return;
    }

    // Open handles to both databases - name map and asset storage
    const auto asset_dbi_result = mdb_dbi_open(txn, g_assets_dbi_name, MDB_CREATE, &desc_.assets_dbi);
    const auto guid_name_dbi_result = mdb_dbi_open(txn, g_guid_to_name_dbi_name, MDB_CREATE, &desc_.name_to_guid_dbi);
    const auto name_guid_dbi_result = mdb_dbi_open(txn, g_name_to_guid_dbi_name, MDB_CREATE, &desc_.name_to_guid_dbi);
    const auto artifacts_dbi_result = mdb_dbi_open(txn, g_hash_to_artifact_dbi_name, MDB_CREATE, &desc_.artifact_dbi);

    if (BEE_LMDB_FAIL(asset_dbi_result) || BEE_LMDB_FAIL(name_guid_dbi_result) || BEE_LMDB_FAIL(artifacts_dbi_result))
    {
        mdb_txn_abort(txn);
        close();
        return;
    }

    BEE_LMDB_ASSERT(mdb_txn_commit(txn));
}

void AssetDatabase::close()
{
    if (desc_.env == nullptr)
    {
        return;
    }

    if (desc_.assets_dbi != g_invalid_dbi)
    {
        mdb_dbi_close(desc_.env, desc_.assets_dbi);
    }

    if (desc_.name_to_guid_dbi != g_invalid_dbi)
    {
        mdb_dbi_close(desc_.env, desc_.name_to_guid_dbi);
    }

    if (desc_.artifact_dbi != g_invalid_dbi)
    {
        mdb_dbi_close(desc_.env, desc_.artifact_dbi);
    }

    mdb_env_close(desc_.env);
    desc_.env = nullptr;
}

AssetDbTxn AssetDatabase::read()
{
    BEE_ASSERT(is_open());
    desc_.kind = AssetDbTxnKind::read_only;
    return { desc_ };
}

AssetDbTxn AssetDatabase::write()
{
    BEE_ASSERT(is_open());
    desc_.kind = AssetDbTxnKind::read_write;
    return { desc_ };
}


} // namespace bee