/*
 *  AssetDatabase.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipeline/AssetDatabase.hpp"
#include "Bee/Core/Memory/Memory.hpp"

#include <lmdb.h>

namespace bee {


#define BEE_LMDB_FAIL(lmdb_result) BEE_FAIL_F(lmdb_result == 0, "LMDB error (%d): %s", lmdb_result, mdb_strerror(lmdb_result))

#define BEE_LMDB_ASSERT(lmdb_result) BEE_ASSERT_F(lmdb_result == 0, "LMDB error (%d): %s", lmdb_result, mdb_strerror(lmdb_result))


AssetDBTxn::AssetDBTxn(const AssetDBTxnType type, MDB_env* env, const u32 asset_dbi)
    : type_(type),
      env_(env),
      asset_dbi_(asset_dbi)
{
    BEE_ASSERT(env != nullptr);

    if (BEE_LMDB_FAIL(mdb_txn_begin(env, nullptr, type == AssetDBTxnType::read_only ? MDB_RDONLY : 0, &txn_)))
    {
        type_ = AssetDBTxnType::invalid;
        txn_ = nullptr;
    }
}

AssetDBTxn::AssetDBTxn(AssetDBTxn&& other) noexcept
{
    move_construct(other);
}

AssetDBTxn& AssetDBTxn::operator=(AssetDBTxn&& other) noexcept
{
    move_construct(other);
}

AssetDBTxn::~AssetDBTxn()
{
    if (is_valid())
    {
        abort();
    }
}

void AssetDBTxn::move_construct(AssetDBTxn& other) noexcept
{
    if (is_valid())
    {
        abort();
    }

    type_ = other.type_;
    txn_ = other.txn_;
    env_ = other.env_;
    asset_dbi_ = other.asset_dbi_;
}

MDB_val make_key(const GUID& guid)
{
    MDB_val val{};
    val.mv_size = sizeof(GUID);
    val.mv_data = const_cast<u8*>(guid.data);
    return val;
}

io::MemoryStream AssetDBTxn::read_asset(const GUID& guid)
{
    BEE_ASSERT(is_valid());

    auto key = make_key(guid);
    MDB_val val{};
    const auto result = mdb_get(txn_, asset_dbi_, &key, &val);

    if (result == MDB_NOTFOUND || BEE_LMDB_FAIL(result))
    {
        val.mv_size = 0;
        val.mv_data = nullptr;
    }

    // Opens a read-only stream
    return io::MemoryStream(val.mv_data, val.mv_size);
}

bool AssetDBTxn::update_asset(const GUID& guid, const void* data, const i32 data_size)
{
    BEE_ASSERT(is_valid());

    auto key = make_key(guid);
    MDB_val val{};
    val.mv_size = data_size;
    val.mv_data = const_cast<void*>(data);

    if (BEE_LMDB_FAIL(mdb_put(txn_, asset_dbi_, &key, &val, MDB_RESERVE)))
    {
        return false;
    }

    return true;
}

bool AssetDBTxn::delete_asset(const GUID& guid)
{
    BEE_ASSERT(is_valid());

    auto key = make_key(guid);
    if (BEE_LMDB_FAIL(mdb_del(txn_, asset_dbi_, &key, nullptr)))
    {
        return false;
    }
    return true;
}

void AssetDBTxn::abort()
{
    BEE_ASSERT(is_valid());

    mdb_txn_abort(txn_);
    txn_ = nullptr;
    type_ = AssetDBTxnType::invalid;
}

void AssetDBTxn::commit()
{
    BEE_ASSERT(is_valid());

    mdb_txn_commit(txn_);
    txn_ = nullptr;
    type_ = AssetDBTxnType::invalid;
}



AssetDB::AssetDB(const Path& location, const char* name)
{

}

AssetDBTxn AssetDB::begin_transaction(const AssetDBTxnType type)
{
    return AssetDBTxn(type, env_, asset_dbi_);
}


} // namespace bee
