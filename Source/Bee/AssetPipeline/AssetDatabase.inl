/*
 *  AssetDatabase.inl
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/AssetPipeline/AssetDatabase.hpp"

#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/Memory/LinearAllocator.hpp"
#include "Bee/Core/Memory/ChunkAllocator.hpp"


struct MDB_txn;
struct MDB_val;
struct MDB_env;


namespace bee {


enum class DbMapId
{
    guid_to_asset,
    guid_to_properties,
    guid_to_dependencies,
    guid_to_artifacts,
    guid_to_sub_assets,
    sub_asset_to_owner,
    artifact_to_guid,
    path_to_guid,
    guid_to_path,
    name_to_guid,
    guid_to_name,
    count
};

enum class AssetPropertyOperation
{
    deleted,
    read,
    modified
};

enum class AssetTxnAccess
{
    read_only,
    read_write
};

struct AssetDatabase;

struct AssetTxnData
{
    AssetTxnData*           prev { nullptr };
    AssetTxnData*           next { nullptr };

    i32                     thread { -1 };
    AssetDatabase*          db { nullptr };
    Allocator*              allocator { nullptr };
    AssetTxnAccess          access { AssetTxnAccess::read_only };
    MDB_txn*                handle { nullptr };
};

struct AssetDatabase
{
    struct ThreadData
    {
        ChunkAllocator      txn_allocator;
        LinearAllocator     tmp_allocator;
        DynamicArray<u8>    serialization_buffer;
        AssetTxnData*       transactions;
        AssetTxnData*       gc_transactions { nullptr };
    };

    Path                    location;
    Path                    artifacts_root;
    MDB_env*                env { nullptr };
    unsigned int            db_maps[static_cast<int>(DbMapId::count)];
    RecursiveMutex          gc_mutex;
    FixedArray<ThreadData>  thread_data;
};

struct TempAllocScope
{
    AssetDatabase::ThreadData*  thread { nullptr };
    size_t                      offset { 0 };

    TempAllocScope(AssetDatabase* db);

    ~TempAllocScope();

    inline operator Allocator*()
    {
        return &thread->tmp_allocator;
    }
};

struct ScopedTxn
{
    AssetDatabase*  db { nullptr };
    MDB_txn*        txn { nullptr };

    ScopedTxn(AssetDatabase* new_db, const unsigned int flags = 0);

    ~ScopedTxn();

    bool get(const DbMapId id, MDB_val* key, MDB_val* val);

    bool del(const DbMapId id, MDB_val* key, MDB_val* val);

    bool put(const DbMapId id, MDB_val* key, MDB_val* val, const unsigned int flags);
};

} // namespace bee