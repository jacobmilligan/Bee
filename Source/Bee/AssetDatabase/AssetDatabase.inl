/*
 *  AssetDatabase.inl
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/AssetDatabase/AssetDatabase.hpp"

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
    guid_to_dependencies,
    guid_to_artifacts,
    artifact_to_guid,
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

enum class AssetOp
{
    read,
    modify,
    create,
    deleted
};

struct AssetDatabase;

struct AssetTxnData
{
    AssetTxnData*       prev { nullptr };
    AssetTxnData*       next { nullptr };

    i32                 thread { -1 };
    AssetDatabase*      db { nullptr };
    Allocator*          allocator { nullptr };
    AssetTxnAccess      access { AssetTxnAccess::read_only };
    MDB_txn*            handle { nullptr };

    i32                 asset_count { 0 };
    AssetOp             asset_op[BEE_ASSET_TXN_MAX_ASSETS];
    AssetMetadata       asset_metadata[BEE_ASSET_TXN_MAX_ASSETS];
};

struct ThreadData
{
    ChunkAllocator      txn_allocator;
    LinearAllocator     tmp_allocator;
    DynamicArray<u8>    tmp_buffer;
    AssetTxnData*       transactions;
    AssetTxnData*       gc_transactions { nullptr };
};

struct AssetDatabase
{
    Path                    location;
    Path                    artifacts_root;
    MDB_env*                env { nullptr };
    unsigned int            db_maps[underlying_t(DbMapId::count)];
    RecursiveMutex          gc_mutex;
    FixedArray<ThreadData>  thread_data;
};

struct TempAllocScope
{
    ThreadData* thread { nullptr };
    size_t      offset { 0 };

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