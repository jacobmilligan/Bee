/*
 *  AssetPipeline.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Path.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/GUID.hpp"


struct MDB_txn;

namespace bee {


BEE_REFLECTED_FLAGS(AssetPlatform, u32, serializable)
{
    unknown = 0u,
    windows = 1u << 0u,
    macos   = 1u << 1u,
    linux   = 1u << 2u,
    metal   = 1u << 3u,
    vulkan  = 1u << 4u,
};

constexpr AssetPlatform current_asset_os()
{
#if BEE_OS_WINDOWS == 1
    return AssetPlatform::windows;
#elif BEE_OS_MACOS == 1
    return AssetPlatform::macos;
#elif BEE_OS_LINUX == 1
    return AssetPlatform::linux;
#endif // BEE_OS_*
}

constexpr AssetPlatform current_asset_gfx_backend()
{
#if BEE_CONFIG_METAL_BACKEND == 1
    return AssetPlatform::metal;
#elif BEE_CONFIG_VULKAN_BACKEND == 1
    return AssetPlatform::vulkan;
#else
    return AssetPlatform::unknown
#endif // BEE_CONFIG_*_BACKEND
}

constexpr AssetPlatform default_asset_platform = current_asset_os() | current_asset_gfx_backend();

struct AssetArtifact
{
    u128                content_hash;
    DynamicArray<u8>    buffer;

    explicit AssetArtifact(Allocator* allocator = system_allocator())
        : buffer(allocator)
    {}
};

struct BEE_REFLECT(serializable, version = 1) AssetPipelineContent
{
    BEE_REFLECT(id = 1, added = 1)
    u128                hash;

    BEE_REFLECT(id = 2, added = 1)
    u32                 type_hash { 0 };

    BEE_REFLECT(id = 4, added = 1)
    DynamicArray<u128>  artifacts;

    BEE_REFLECT(id = 5, added = 1)
    DynamicArray<u128>  children;

    explicit AssetPipelineContent(Allocator* allocator = system_allocator())
        : artifacts(allocator)
    {}
};


struct BEE_REFLECT(serializable, version = 1) AssetFile
{
    BEE_REFLECT(id = 1, added = 1)
    GUID                                guid;

    BEE_REFLECT(id = 2, added = 1)
    u128                                content_hash;

    BEE_REFLECT(id = 3, added = 1)
    bool                                is_directory { false };

    BEE_REFLECT(id = 4, added = 1)
    String                              name;

    BEE_REFLECT(id = 5, added = 1)
    Path                                source;

    BEE_REFLECT(id = 6, added = 1)
    DynamicArray<AssetPipelineContent>  assets;

    BEE_REFLECT(id = 7, added = 1)
    DynamicArray<TypeInstance>          options;

    explicit AssetFile(Allocator* allocator = system_allocator())
        : name(allocator),
          source(allocator),
          assets(allocator),
          options(allocator)
    {}
};

struct BEE_REFLECT(serializable, version = 1) AssetDbItem
{
    BEE_REFLECT(id = 1, added = 1)
    u64         src_timestamp { 0 };

    BEE_REFLECT(id = 2, added = 1)
    u64         dst_timestamp { 0 };

    BEE_REFLECT(id = 3, added = 1)
    AssetFile   contents;

    explicit AssetDbItem(Allocator* allocator = system_allocator())
        : contents(allocator)
    {}
};


#define BEE_ASSET_DATABASE_MODULE_NAME "BEE_ASSET_DATABASE_MODULE"

using AssetDBTxnHandle = MDB_txn*;

enum class AssetDbTxnKind
{
    invalid,
    read_only,
    read_write
};

struct AssetDatabaseModule;

struct AssetDbTxn final : public Noncopyable
{
    AssetDbTxnKind          kind { AssetDbTxnKind::invalid };
    AssetDBTxnHandle        handle { nullptr };
    AssetDatabaseModule*    assetdb { nullptr };

    AssetDbTxn() = default;

    AssetDbTxn(AssetDbTxn&& other) noexcept;

    ~AssetDbTxn();

    AssetDbTxn& operator=(AssetDbTxn&& other) noexcept;
};


struct AssetDatabaseModule
{
    void (*open)(const Path& directory, const StringView& name) { nullptr };

    void (*close)() { nullptr };

    bool (*is_open)() { nullptr };

    const Path& (*location)() { nullptr };

    AssetDbTxn (*read)() { nullptr };

    AssetDbTxn (*write)() { nullptr };

    void (*abort_transaction)(AssetDbTxn* txn) { nullptr };

    void (*commit_transaction)(AssetDbTxn* txn) { nullptr };

    bool (*put_asset)(const AssetDbTxn& txn, AssetDbItem* asset) {nullptr };

    bool (*delete_asset)(const AssetDbTxn& txn, const GUID& guid) { nullptr };

    bool (*get_asset)(const AssetDbTxn& txn, const GUID& guid, AssetDbItem* asset) {nullptr };

    bool (*has_asset)(const AssetDbTxn& txn, const GUID& guid) { nullptr };

    bool (*set_asset_name)(const AssetDbTxn& txn, const GUID& guid, const StringView& name) { nullptr };

    bool (*get_name_from_guid)(const AssetDbTxn& txn, const GUID& guid, String* name) { nullptr };

    bool (*get_guid_from_name)(const AssetDbTxn& txn, const StringView& name, GUID* guid) { nullptr };

    bool (*get_asset_from_path)(const AssetDbTxn& txn, const Path& path, AssetDbItem* asset) { nullptr };

    bool (*put_artifact)(const AssetDbTxn& txn, const AssetArtifact& artifact) { nullptr };

    bool (*delete_artifact)(const AssetDbTxn& txn, const u128& hash) { nullptr };

    bool (*get_artifact_path)(const AssetDbTxn& txn, const u128& hash, Path* dst) { nullptr };

    bool (*get_artifact)(const AssetDbTxn& txn, const u128& hash, AssetArtifact* artifact) { nullptr };

    bool (*get_artifacts_from_guid)(const AssetDbTxn& txn, const GUID& guid, DynamicArray<AssetArtifact>* result) { nullptr };
};


enum class BEE_REFLECT() AssetCompilerStatus
{
    success,
    fatal_error,
    unsupported_platform,
    unsupported_filetype,
    invalid_source_format,
    unknown
};

enum class DeleteAssetKind
{
    asset_only,
    asset_and_source
};

BEE_RAW_HANDLE_I32(AssetPipelineContentHandle);

class AssetCompilerContext
{
public:
    AssetCompilerContext(const AssetPlatform platform, const StringView& location, const StringView& cache_dir, const TypeInstance& options, DynamicArray<AssetPipelineContent>* content_out, Allocator* allocator)
        : platform_(platform),
          location_(location),
          cache_dir_(cache_dir),
          options_(options),
          assets_(content_out),
          allocator_(allocator)
    {}

    AssetPipelineContentHandle add_content(const Type* type)
    {
        assets_->emplace_back(allocator_);

        auto& asset = assets_->back();
        asset.type_hash = type->hash;

        return AssetPipelineContentHandle(assets_->size() - 1);
    }

    AssetPipelineContentHandle add_child(const AssetPipelineContentHandle& parent, const Type* child_type)
    {
        auto& parent_asset = assets_[parent.id];
        parent_asset.emplace_back()
        assets_->emplace_back(allocator_);
    }

    AssetArtifact& add_artifact()

    inline AssetPlatform platform() const
    {
        return platform_;
    }

    inline const StringView& location() const
    {
        return location_;
    }

    inline const StringView& cache_directory() const
    {
        return cache_dir_;
    }

    inline Allocator* temp_allocator()
    {
        return allocator_;
    }

    inline const DynamicArray<AssetPipelineContent>& assets() const
    {
        return *assets_;
    }

    inline const DynamicArray<AssetArtifact>& artifacts() const
    {
        return *artifacts_;
    }

    template <typename OptionsType>
    const OptionsType& options() const
    {
        return *options_.get<OptionsType>();
    }
private:
    AssetPlatform                       platform_ { AssetPlatform::unknown };
    StringView                          location_;
    StringView                          cache_dir_;
    const TypeInstance&                 options_;
    Allocator*                          allocator_ { nullptr };
    DynamicArray<AssetPipelineContent>* assets_ { nullptr };
    DynamicArray<AssetArtifact>*        artifacts_ { nullptr };
};


struct AssetCompilerData;

struct AssetCompiler
{
    AssetCompilerData* data { nullptr };

    const char* (*get_name)() { nullptr };

    Span<const char* const> (*supported_file_types)() { nullptr };

    const Type* (*options_type)() { nullptr };

    void (*init)(AssetCompilerData* data, const i32 thread_count) { nullptr };

    void (*destroy)(AssetCompilerData* data) { nullptr };

    AssetCompilerStatus (*compile)(AssetCompilerData* data, const i32 thread_index, AssetCompilerContext* ctx) { nullptr };
};


#define BEE_ASSET_PIPELINE_MODULE_NAME "BEE_ASSET_PIPELINE_MODULE"

struct AssetPipelineInitInfo
{
    AssetPlatform   platform { AssetPlatform::unknown };
    Path            project_root;
    Path            cache_directory;
    const char*     asset_database_name { nullptr };
};

struct AssetPipeline;

struct AssetPipelineModule
{
    bool (*init)(const AssetPipelineInitInfo& info) { nullptr };

    void (*destroy)() { nullptr };

    void (*set_platform)(const AssetPlatform platform) { nullptr };

    void (*import_asset)(const Path& source_path, const Path& dst_path, const StringView& name) { nullptr };

    void (*delete_asset)(const GUID& guid, const DeleteAssetKind kind) { nullptr };

    void (*delete_asset_with_name)(const StringView& name, const DeleteAssetKind kind) { nullptr };

    void (*delete_asset_at_path)(const Path& path, const DeleteAssetKind kind) { nullptr };

    void (*register_compiler)(AssetCompiler* compiler) { nullptr };

    void (*unregister_compiler)(AssetCompiler* compiler) { nullptr };

    void (*add_asset_directory)(const Path& path) { nullptr };

    void (*remove_asset_directory)(const Path& path) { nullptr };

    Span<const Path> (*asset_directories)() { nullptr };

    void (*refresh)() { nullptr };
};


} // namespace bee

#include "Bee/Plugins/AssetPipeline/AssetPipeline.inl"

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.AssetPipeline/AssetPipeline.generated.inl"
#endif // BEE_ENABLE_REFLECTION
