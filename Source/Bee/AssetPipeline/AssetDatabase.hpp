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

enum class BEE_REFLECT(serializable) AssetFileKind : u8
{
    unknown,
    file,
    directory,
    sub_asset
};

#define BEE_ASSET_DATABASE_MODULE_NAME "BEE_ASSET_DATABASE"


struct AssetDatabase;
struct AssetDatabaseModule;
struct AssetTxnData;


struct AssetInfo
{
    GUID            guid;
    u64             timestamp { 0 };
    u64             meta_timestamp { 0 };
    AssetFileKind   kind { AssetFileKind::unknown };
    u32             importer { 0 };
};

struct AssetName
{
    size_t      size { 0 };
    const void* data { nullptr };

    AssetName() = default;

    AssetName(const StringView& name)
        : size(name.size()),
          data(name.data())
    {}

    StringView to_string() const
    {
        return StringView(static_cast<const char*>(data), size);
    }

//    template <typename T>
//    explicit AssetBlob(const T& src_data)
//        : size(sizeof(T)),
//          data(&src_data)
//    {}
};

struct AssetArtifact
{
    u128    content_hash;
    u32     type_hash { 0 };
    u32     key { 0 };
};

struct AssetDatabaseError
{
    enum Enum
    {
        txn_max_asset_ops,
        invalid_properties_handle,
        deleted_properties_handle,
        invalid_access,
        not_found,
        failed_to_write_artifact_to_disk,
        lmdb_error,
        unknown
    };

    BEE_ENUM_STRUCT(AssetDatabaseError);

    const char* to_string() const
    {
        BEE_TRANSLATION_TABLE(value, Enum, const char*, Enum::unknown,
            "Transaction has reached the maximum number asset modifications and creations", // txn_max_asset_ops,
            "Asset properties handle was invalid", // invalid_properties_handle,
            "Asset properties handle points to a deleted asset", // deleted_properties_handle,
            "Attempted to modify an asset in a read-only transaction", // invalid_access,
            "Asset not found", // not_found,
            "Failed to write artifact buffer to disk", // failed_to_write_artifact_to_disk,
            "LMDB error"// lmdb_error
        );
    }
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
    AssetDatabase* (*open)(const PathView& location) { nullptr };

    void (*close)(AssetDatabase* db) { nullptr };

    bool (*is_open)(AssetDatabase* db) { nullptr };

    PathView (*location)(AssetDatabase* db) { nullptr };

    // Call at regular intervals to garbage-collect old transaction memory
    void (*gc)(AssetDatabase* db) { nullptr };

    AssetTxn (*read)(AssetDatabase* db) { nullptr };

    AssetTxn (*write)(AssetDatabase* db) { nullptr };

    void (*abort)(AssetTxn* txn) { nullptr };

    bool (*commit)(AssetTxn* txn) { nullptr };

    bool (*is_valid_txn)(AssetTxn* txn) { nullptr };

    bool (*is_read_only)(AssetTxn* txn) { nullptr };

    bool (*asset_exists)(AssetTxn* txn, const GUID guid) { nullptr };


    Result<AssetInfo*, AssetDatabaseError> (*create_asset)(AssetTxn* txn) { nullptr };

    Result<void, AssetDatabaseError> (*delete_asset)(AssetTxn* txn, const GUID guid) { nullptr };

    Result<AssetInfo, AssetDatabaseError> (*get_asset_info)(AssetTxn* txn, const GUID guid) { nullptr };

    Result<void, AssetDatabaseError> (*set_asset_info)(AssetTxn* txn, const AssetInfo& info) { nullptr };


    Result<TypeInstance, AssetDatabaseError> (*get_import_settings)(AssetTxn* txn, const GUID guid, Allocator* allocator) { nullptr };

    Result<void, AssetDatabaseError> (*set_import_settings)(AssetTxn* txn, const GUID guid, const TypeInstance& settings) { nullptr };


    Result<void, AssetDatabaseError> (*set_asset_path)(AssetTxn* txn, const GUID guid, const StringView path) { nullptr };

    Result<StringView, AssetDatabaseError> (*get_asset_path)(AssetTxn* txn, const GUID guid) { nullptr };

    Result<GUID, AssetDatabaseError> (*get_guid_from_path)(AssetTxn* txn, const StringView path) { nullptr };


    Result<void, AssetDatabaseError> (*set_asset_name)(AssetTxn* txn, const GUID guid, const AssetName& name) { nullptr };

    Result<AssetName, AssetDatabaseError> (*get_asset_name)(AssetTxn* txn, const GUID guid) { nullptr };

    Result<GUID, AssetDatabaseError> (*get_guid_from_name)(AssetTxn* txn, const AssetName& name) { nullptr };


    u128 (*get_artifact_hash)(const void* buffer, const size_t buffer_size) { nullptr };

    void (*get_artifact_path)(AssetTxn* txn, const u128& hash, Path* dst) { nullptr };

    Result<u128, AssetDatabaseError> (*add_artifact)(AssetTxn* txn, const GUID guid, const Type artifact_type, const void* buffer, const size_t buffer_size) { nullptr };

    Result<u128, AssetDatabaseError> (*add_artifact_with_key)(AssetTxn*, const GUID guid, const Type artifact_type, const u32 key, const void* buffer, const size_t buffer_size) { nullptr };

    Result<void, AssetDatabaseError> (*remove_artifact)(AssetTxn* txn, const GUID guid, const u128& hash) { nullptr };

    Result<void, AssetDatabaseError> (*remove_all_artifacts)(AssetTxn* txn, const GUID guid) { nullptr };

    Result<i32, AssetDatabaseError> (*get_artifacts)(AssetTxn* txn, const GUID guid, AssetArtifact* dst) { nullptr };

    Result<void, AssetDatabaseError> (*add_dependency)(AssetTxn* txn, const GUID guid, const GUID dependency) { nullptr };

    Result<void, AssetDatabaseError> (*remove_dependency)(AssetTxn* txn, const GUID guid, const GUID dependency) { nullptr };

    Result<void, AssetDatabaseError> (*remove_all_dependencies)(AssetTxn* txn, const GUID guid) { nullptr };

    Result<void, AssetDatabaseError> (*set_sub_asset)(AssetTxn* txn, const GUID owner, const GUID sub_asset) { nullptr };

    Result<void, AssetDatabaseError> (*remove_all_sub_assets)(AssetTxn* txn, const GUID owner) { nullptr };

    Result<i32, AssetDatabaseError> (*get_sub_assets)(AssetTxn* txn, const GUID guid, GUID* dst) { nullptr };
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
    #include "Bee.AssetPipeline/AssetDatabase.generated.inl"
#endif // BEE_ENABLE_REFLECTION