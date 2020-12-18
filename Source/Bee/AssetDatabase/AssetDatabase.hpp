/*
 *  AssetDatabase.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Path.hpp"
#include "Bee/Core/GUID.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"
#include "Bee/Core/Result.hpp"
#include "Bee/Core/Handle.hpp"


namespace bee {


#ifndef BEE_ASSET_TXN_MAX_ASSETS
    #define BEE_ASSET_TXN_MAX_ASSETS 128
#endif // BEE_ASSET_TXN_MAX_ASSETS

enum class AssetDatabaseStatus : int
{
    txn_max_asset_ops,
    invalid_properties_handle,
    deleted_properties_handle,
    invalid_access,
    not_found,
    failed_to_write_artifact_to_disk,
    internal_error
};

enum class BEE_REFLECT(serializable) AssetFileKind : u8
{
    unknown,
    file,
    directory
};

#define BEE_ASSET_DATABASE_MODULE_NAME "BEE_ASSET_DATABASE"


struct AssetDatabase;
struct AssetDatabaseModule;
struct AssetTxnData;


struct BEE_REFLECT(serializable, version = 1) AssetMetadata
{
    BEE_REFLECT(id = 1, added = 1)
    GUID            guid;

    BEE_REFLECT(id = 2, added = 1)
    u64             timestamp { 0 };

    BEE_REFLECT(id = 3, added = 1)
    AssetFileKind   kind { AssetFileKind::unknown };

    BEE_REFLECT(id = 4, added = 1)
    Path            source;

    BEE_REFLECT(id = 5, added = 1)
    TypeInstance    properties;

    BEE_REFLECT(id = 6, added = 1)
    u32             importer { 0 };

    AssetMetadata(Allocator* allocator = system_allocator())
        : source(allocator)
    {}
};

struct AssetDatabaseError
{
    AssetDatabaseStatus status { AssetDatabaseStatus::internal_error };
    const char*         message { nullptr };
};

class AssetTxn final : public Noncopyable
{
public:
    AssetTxn(AssetDatabaseModule* module, AssetTxnData* txn)
        : module_(module),
          txn_(txn)
    {}

    AssetTxn(AssetTxn&& other) noexcept
    {
        move_construct(other);
    }

    inline ~AssetTxn();

    AssetTxn& operator=(AssetTxn&& other) noexcept
    {
        move_construct(other);
        return *this;
    }

    AssetTxnData* data()
    {
        return txn_;
    }

    inline bool commit();

    inline void abort();
private:
    AssetDatabaseModule*    module_ { nullptr };
    AssetTxnData*           txn_ { nullptr };

    void move_construct(AssetTxn& other) noexcept
    {
        module_ = other.module_;
        txn_ = other.txn_;
        other.module_ = nullptr;
        other.txn_ = nullptr;
    }
};


template <typename T>
using AssetDatabaseResult = Result<T, AssetDatabaseError>;

struct AssetDatabaseModule
{
    AssetDatabase* (*open)(const Path& location) { nullptr };

    void (*close)(AssetDatabase* db) { nullptr };

    bool (*is_open)(AssetDatabase* db) { nullptr };

    const Path& (*location)(AssetDatabase* db) { nullptr };

    // Call at regular intervals to garbage-collect old transaction memory
    void (*gc)(AssetDatabase* db) { nullptr };

    AssetTxn (*read)(AssetDatabase* db) { nullptr };

    AssetTxn (*write)(AssetDatabase* db) { nullptr };

    void (*abort)(AssetTxn* txn) { nullptr };

    bool (*commit)(AssetTxn* txn) { nullptr };

    bool (*is_valid_txn)(AssetTxn* txn) { nullptr };

    bool (*is_read_only)(AssetTxn* txn) { nullptr };

    bool (*asset_exists)(AssetTxn* txn, const GUID guid) { nullptr };

    Result<AssetMetadata*, AssetDatabaseError> (*create_asset)(AssetTxn* txn, const Type type) { nullptr };

    bool (*delete_asset)(AssetTxn* txn, const GUID guid) { nullptr };

    Result<const AssetMetadata*, AssetDatabaseError> (*read_asset)(AssetTxn* txn, const GUID guid) { nullptr };

    Result<AssetMetadata*, AssetDatabaseError> (*modify_asset)(AssetTxn* txn, const GUID guid) { nullptr };

    Result<const AssetMetadata*, AssetDatabaseError> (*read_serialized_asset)(AssetTxn* txn, Serializer* serializer) { nullptr };

    Result<AssetMetadata*, AssetDatabaseError> (*modify_serialized_asset)(AssetTxn* txn, Serializer* serializer) { nullptr };

    u128 (*get_artifact_hash)(const void* buffer, const size_t buffer_size) { nullptr };

    Result<u128, AssetDatabaseError> (*add_artifact)(AssetTxn* txn, const GUID guid, const Type artifact_type, const void* buffer, const size_t buffer_size) { nullptr };

    bool (*remove_artifact)(AssetTxn* txn, const GUID guid, const u128& hash) { nullptr };

    bool (*add_dependency)(AssetTxn* txn, const GUID parent, const GUID child) { nullptr };

    bool (*remove_dependency)(AssetTxn* txn, const GUID parent, const GUID child) { nullptr };
};

AssetTxn::~AssetTxn()
{
    if (module_ == nullptr || txn_ == nullptr)
    {
        return;
    }

    if (module_->is_valid_txn(this))
    {
        module_->abort(this);
    }

    module_ = nullptr;
    txn_ = nullptr;
}

bool AssetTxn::commit()
{
    if (module_ == nullptr || txn_ == nullptr)
    {
        return false;
    }

    return module_->commit(this);
}

void AssetTxn::abort()
{
    if (module_ == nullptr || txn_ == nullptr)
    {
        return;
    }

    module_->abort(this);
}



} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.AssetDatabase/AssetDatabase.generated.inl"
#endif // BEE_ENABLE_REFLECTION