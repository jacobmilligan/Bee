/*
 *  AssetDatabase.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/GUID.hpp"
#include "Bee/Core/IO.hpp"

struct MDB_txn;
struct MDB_env;

namespace bee {


enum class AssetDBTxnType
{
    invalid,
    read_only,
    write
};

class BEE_DEVELOP_API AssetDBTxn final : public Noncopyable
{
public:
    AssetDBTxn(const AssetDBTxnType type, MDB_env* env, const u32 asset_dbi);

    AssetDBTxn(AssetDBTxn&& other) noexcept;

    ~AssetDBTxn();

    AssetDBTxn& operator=(AssetDBTxn&& other) noexcept;

    io::MemoryStream read_asset(const GUID& guid);

    bool update_asset(const GUID& guid, const void* data, const i32 data_size);

    bool delete_asset(const GUID& guid);

    void abort();

    void commit();

    inline bool is_valid() const
    {
        return type_ != AssetDBTxnType::invalid && txn_ != nullptr;
    }
private:
    AssetDBTxnType  type_ { AssetDBTxnType::invalid };
    MDB_txn*        txn_ { nullptr };
    MDB_env*        env_ { nullptr };
    u32             asset_dbi_ { 0 };

    void move_construct(AssetDBTxn& other) noexcept;
};

class BEE_DEVELOP_API AssetDB
{
public:
    AssetDB(const Path& location, const char* name);

    AssetDBTxn begin_transaction(const AssetDBTxnType type);

private:
    MDB_env*    env_ { nullptr };
    u32         asset_dbi_ { 0 };
};


} // namespace bee