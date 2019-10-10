/*
 *  AssetDatabase.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/GUID.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/AssetPipeline/AssetMeta.hpp"


struct MDB_txn;
struct MDB_env;

namespace bee {


enum class AssetDBTxnType
{
    invalid,
    read_only,
    write
};


class BEE_DEVELOP_API AssetDB
{
public:
    ~AssetDB();

    bool open(const char* assets_root, const char* location, const char* name);

    void close();

    bool asset_exists(const GUID& guid);

    bool put_asset(const GUID& guid, const char* src_path);

    bool put_asset(const AssetMeta& meta, const char* src_path);

    bool get_asset(const GUID& guid, AssetMeta* meta);

    bool get_source_path(const GUID& guid, Path* path);

    bool get_artifact_path(const GUID& guid, Path* path);

    bool delete_asset(const GUID& guid);

    bool set_asset_name(const GUID& guid, const StringView& name);

    bool erase_asset_name(const GUID& guid);

    bool get_asset_name(const GUID& guid, io::StringStream* dst);

    bool get_asset_guid(const StringView& name, GUID* guid);
private:
    static constexpr const char* asset_dbi_name_ = "AssetStorage";
    static constexpr const char* name_dbi_name_ = "NameMap";
    static constexpr u32 invalid_dbi_ = limits::max<u32>();

    struct Transaction
    {
        MDB_txn* ptr { nullptr };

        explicit Transaction(MDB_env* env, const unsigned long flags = 0);

        ~Transaction();

        void commit();

        void abort();

        inline MDB_txn* operator*()
        {
            return ptr;
        }
    };

    MDB_env*    env_ { nullptr };
    u32         asset_dbi_ { invalid_dbi_ };
    u32         name_dbi_ { invalid_dbi_ };
    Path        assets_root_;
    Path        path_;
    Path        artifacts_root_;

    bool is_valid();

    bool set_asset_name(Transaction& txn, const GUID& guid, const StringView& name);

    bool get_asset(Transaction& txn, const GUID& guid, AssetMeta* meta, const char** src_path);

    bool put_asset(Transaction& txn, const AssetMeta& meta, const char* src_path);
};


} // namespace bee