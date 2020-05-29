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
#include "Bee/Core/Containers/HashMap.hpp"


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


BEE_RAW_HANDLE_U32(AssetCompilerId) BEE_REFLECT(serializable);


struct BEE_REFLECT(serializable, version = 1) AssetMetadata
{
    BEE_REFLECT(id = 1, added = 1)
    GUID                guid;

    BEE_REFLECT(id = 2, added = 1)
    bool                is_directory { false };

    BEE_REFLECT(id = 3, added = 1)
    AssetCompilerId     compiler;

    BEE_REFLECT(id = 4, added = 1)
    TypeInstance        settings;
};


struct BEE_REFLECT(serializable, version = 1) AssetArtifact
{
    BEE_REFLECT(id = 3, added = 1)
    u128    content_hash;

    BEE_REFLECT(id = 4, added = 1)
    u32     type_hash { 0 };
};


struct BEE_REFLECT(serializable, version = 1) CompiledAsset
{
    BEE_REFLECT(id = 1, added = 1)
    u64             src_timestamp { 0 };

    BEE_REFLECT(id = 2, added = 1)
    u64             metadata_timestamp { 0 };

    BEE_REFLECT(id = 3, added = 1)
    u128            source_hash;

    BEE_REFLECT(id = 4, added = 1)
    AssetArtifact   main_artifact;

    BEE_REFLECT(id = 5, added = 1)
    String          uri;

    BEE_REFLECT(id = 6, added = 1)
    AssetMetadata   metadata;

    explicit CompiledAsset(Allocator* allocator = system_allocator())
        : uri(allocator)
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

struct AssetDatabaseEnv;
struct AssetDatabaseModule;

struct AssetDbTxn final : public Noncopyable
{
    AssetDbTxnKind          kind { AssetDbTxnKind::invalid };
    AssetDBTxnHandle        handle { nullptr };
    AssetDatabaseModule*    assetdb { nullptr };
    AssetDatabaseEnv*       env { nullptr };

    AssetDbTxn() = default;

    AssetDbTxn(AssetDbTxn&& other) noexcept;

    ~AssetDbTxn();

    AssetDbTxn& operator=(AssetDbTxn&& other) noexcept;
};


struct AssetDatabaseModule
{
    AssetDatabaseEnv* (*open)(const Path& directory, const StringView& name, Allocator* allocator) { nullptr };

    void (*close)(AssetDatabaseEnv* env) { nullptr };

    bool (*is_open)(AssetDatabaseEnv* env) { nullptr };

    const Path& (*location)(AssetDatabaseEnv* env) { nullptr };

    AssetDbTxn (*read)(AssetDatabaseEnv* env) { nullptr };

    AssetDbTxn (*write)(AssetDatabaseEnv* env) { nullptr };

    void (*abort)(AssetDatabaseEnv* env, AssetDbTxn* txn) { nullptr };

    void (*commit)(AssetDatabaseEnv* env, AssetDbTxn* txn) { nullptr };

    /*
     * Asset data
     */
    bool (*put_asset)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, CompiledAsset* asset) {nullptr };

    bool (*delete_asset)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid) { nullptr };

    bool (*get_asset)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, CompiledAsset* asset) {nullptr };

    bool (*get_asset_from_path)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const StringView& uri, CompiledAsset* asset) {nullptr };

    i32 (*get_guids_by_type)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const TypeRef& type, GUID* dst) { nullptr };

    bool (*has_asset)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid) { nullptr };

    /*
     * Asset dependencies
     */
    bool (*set_asset_dependencies)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, const GUID* dependencies, const i32 dependency_count) { nullptr };

    i32 (*get_asset_dependencies)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, GUID* dst) { nullptr };

    /*
     * Artifacts
     */
    bool (*put_artifact)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, const AssetArtifact& artifact, const void* buffer, const size_t buffer_size) { nullptr };

    bool (*delete_artifact)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, const u128& hash) { nullptr };

    bool (*get_artifact)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const u128& hash, AssetArtifact* dst, io::FileStream* dst_stream) { nullptr };

    bool (*has_artifact)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const u128& hash) { nullptr };

    i32 (*get_artifacts_from_guid)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const GUID& guid, AssetArtifact* dst) { nullptr };

    i32 (*get_guids_from_artifact)(AssetDatabaseEnv* env, const AssetDbTxn& txn, const u128& hash, GUID* dst) { nullptr };
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

struct AssetCompilerOutput
{
    DynamicArray<TypeRef>*              artifact_types { nullptr };
    DynamicArray<DynamicArray<u8>>*     artifact_buffers { nullptr };
    DynamicArray<GUID>*                 dependencies { nullptr };
};

class AssetCompilerContext
{
public:
    AssetCompilerContext(const AssetPlatform platform, const StringView& location, const StringView& cache_dir, const TypeInstance& options, const AssetCompilerOutput& output, Allocator* allocator)
        : platform_(platform),
          location_(location),
          cache_dir_(cache_dir),
          options_(options),
          output_(output),
          allocator_(allocator)
    {}

    template <typename T>
    inline DynamicArray<u8>& add_artifact()
    {
        const auto type = get_type<T>();

        BEE_ASSERT_F(!type->is(TypeKind::unknown), "Artifact type must be reflected using BEE_REFLECT()");

        output_.artifact_buffers->emplace_back(allocator_);
        output_.artifact_types->push_back(type);
        return output_.artifact_buffers->back();
    }

    inline void set_main(const DynamicArray<u8>& buffer)
    {
        const auto index = find_index_if(*output_.artifact_buffers, [&](const DynamicArray<u8>& b)
        {
            return b.data() == buffer.data();
        });

        if (index < 0)
        {
            log_error("Invalid artifact buffer - must have been created using add_artifact");
            return;
        }

        main_artifact_ = index;
    }

    void add_dependency(const GUID& guid) const
    {
        if (find_index(*output_.dependencies, guid) >= 0)
        {
            log_error("Asset already has a dependency with GUID %s", format_guid(guid, GUIDFormat::digits));
            return;
        }

        output_.dependencies->push_back(guid);
    }

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

    template <typename OptionsType>
    const OptionsType& options() const
    {
        return *options_.get<OptionsType>();
    }

    inline i32 main_artifact() const
    {
        return main_artifact_;
    }

private:
    AssetPlatform                       platform_ { AssetPlatform::unknown };
    StringView                          location_;
    StringView                          cache_dir_;
    const TypeInstance&                 options_;
    AssetCompilerOutput                 output_;
    i32                                 main_artifact_ { -1 };
    Allocator*                          allocator_ { nullptr };
};


struct AssetCompilerData;

struct AssetCompiler
{
    AssetCompilerData* data { nullptr };

    const char* (*get_name)() { nullptr };

    Span<const char* const> (*supported_file_types)() { nullptr };

    TypeRef (*asset_type)() { nullptr };

    TypeRef (*settings_type)() { nullptr };

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

struct BEE_REFLECT(serializable) ManifestFile
{
    String                          name;
    DynamicHashMap<String, String>  assets;

    explicit ManifestFile(Allocator* allocator = system_allocator())
        : name(allocator),
          assets(allocator)
    {}
};

struct AssetPipeline;

struct AssetPipelineModule
{
    AssetPipeline* (*init)(const AssetPipelineInitInfo& info, Allocator* allocator) { nullptr };

    void (*destroy)(AssetPipeline* instance) { nullptr };

    void (*set_platform)(AssetPipeline* instance, const AssetPlatform platform) { nullptr };

    void (*import_asset)(AssetPipeline* instance, const Path& source_path) { nullptr };

    void (*reimport_asset)(AssetPipeline* instance, const GUID& guid) { nullptr };

    // TODO(Jacob): set_compiler(id) instead of import_asset/import_asset_default
    //  and add a get_compilers_for_asset(GUID, AssetCompilerId*)

    void (*delete_asset)(AssetPipeline* instance, const GUID& guid, const DeleteAssetKind kind) { nullptr };

    void (*delete_asset_at_path)(AssetPipeline* instance, const StringView& uri, const DeleteAssetKind kind) { nullptr };

    void (*add_root)(AssetPipeline* instance, const GUID& guid) { nullptr };

    void (*remove_root)(AssetPipeline* instance, const GUID& guid) { nullptr };

    void (*add_asset_directory)(AssetPipeline* instance, const Path& path) { nullptr };

    void (*remove_asset_directory)(AssetPipeline* instance, const Path& path) { nullptr };

    Span<const Path> (*asset_directories)(AssetPipeline* instance) { nullptr };

    void (*refresh)(AssetPipeline* instance) { nullptr };

    // Compilers
    void (*register_compiler)(AssetCompiler* compiler) { nullptr };

    void (*unregister_compiler)(AssetCompiler* compiler) { nullptr };

    i32 (*get_compilers_for_filetype)(const StringView& extension, AssetCompilerId* dst_buffer) { nullptr };
};


} // namespace bee

#include "Bee/Plugins/AssetPipeline/AssetPipeline.inl"

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.AssetPipeline/AssetPipeline.generated.inl"
#endif // BEE_ENABLE_REFLECTION
