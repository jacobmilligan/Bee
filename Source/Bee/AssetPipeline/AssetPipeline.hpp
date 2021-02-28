/*
 *  AssetLoader.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/StaticArray.hpp"
#include "Bee/Core/Serialization/BinarySerializer.hpp"

#include "Bee/AssetPipeline/AssetDatabase.hpp"


namespace bee {


struct AssetPipeline;
struct AssetPipelineModule;

struct AssetPipelineError
{
    enum Enum
    {
        unknown,

        import,
        asset_database,
        unsupported_file_type,
        failed_to_write_metadata,
        importer_registered,
        importer_not_registerd,
        failed_to_create_asset,
        failed_to_import,
        failed_to_write_artifacts,
        failed_to_update_dependencies,
        failed_to_update_sub_assets,

        load,
        failed_to_locate,
        no_loader_for_type,
        failed_to_allocate,
        failed_to_load,
        failed_to_unload,
        invalid_asset_handle,
        missing_data,
        invalid_data,
        loader_type_conflict,
        invalid_loader,
        count
    };

    BEE_ENUM_STRUCT(AssetPipelineError);

    const char* to_string() const
    {
        BEE_TRANSLATION_TABLE(value, Enum, const char*, Enum::count,
            "Unknown Asset Load Pipeline error",                                // unknown

            "Import stage is not enabled",                                      // import
            "Failed to open or close asset database",                           // asset_database
            "Unsupported file type",                                            // unsupported_file_type
            "Failed to write metadata to disk",                                 // failed_to_write_metadata
            "Importer is already registered to the Asset Pipeline",             // importer_registered
            "Importer is not registered to the Asset Pipeline",                 // importer_not_registered
            "Failed to create new asset",                                       // failed_to_create_asset
            "Failed to import asset",                                           // failed_to_import
            "Failed to write artifacts",                                        // failed_to_write_artifacts
            "Failed to update dependencies",                                    // failed_to_update_dependencies
            "Failed to update sub_assets",                                      // failed_to_update_sub_assets

            "Load stage is not enabled",                                        // load
            "Failed to locate asset from GUID",                                 // failed_to_locate
            "Unable to find a loader registered for the located asset type",    // no_loader_for_type
            "Failed to allocate asset data",                                    // failed_to_allocate
            "Failed to load asset data",                                        // failed_to_load
            "Failed to unload asset data",                                      // failed_to_unload
            "Invalid asset handle",                                             // invalid_asset_handle
            "Content hash for asset resolved to missing data",                  // missing_data
            "Asset data has an invalid format or is corrupted",                 // invalid_data
            "A loader is already registered for that asset type",               // loader_type_conflict
            "The AssetLoader is not a valid or registered loader",              // invalid_loader
        )
    }
};

/*
 **********************************
 *
 * Asset import API
 *
 **********************************
 */
BEE_REFLECTED_FLAGS(AssetPlatform, u32, serializable)
{
    unknown = 0u,
    windows = 1u << 0u,
    macos   = 1u << 1u,
    linux   = 1u << 2u,
    metal   = 1u << 3u,
    vulkan  = 1u << 4u,
};

struct BEE_REFLECT(version = 1, serializable) AssetMetadata
{
    BEE_REFLECT(added = 1)
    GUID            guid;

    BEE_REFLECT(added = 1)
    u32             importer { 0 };

    BEE_REFLECT(added = 1)
    AssetFileKind   kind { AssetFileKind::unknown };

    BEE_REFLECT(added = 1)
    TypeInstance    settings;
};


struct AssetImportContext
{
    Allocator*              temp_allocator { nullptr };
    AssetPlatform           target_platforms { AssetPlatform::unknown };
    StringView              target_platform_string;
    GUID                    guid;
    AssetDatabaseModule*    db { nullptr };
    AssetTxn*               txn { nullptr };
    DynamicArray<u8>*       artifact_buffer { nullptr };
    PathView                path;
    PathView                cache_root;
    u32                     importer_hash { 0 };
    TypeInstance*           settings;

    inline Result<u128, AssetDatabaseError> add_artifact(const Type type, const void* buffer, const size_t buffer_size, const u32 key = 0)
    {
        return db->add_artifact_with_key(txn, guid, type, key, artifact_buffer->data(), artifact_buffer->size());
    }

    template <typename T>
    inline Result<u128, AssetDatabaseError> add_artifact(const T* artifact, const u32 key = 0)
    {
        BinarySerializer serializer(artifact_buffer);
        serialize(SerializerMode::writing, &serializer, const_cast<T*>(artifact), temp_allocator);
        return add_artifact(get_type<T>(), artifact_buffer->data(), artifact_buffer->size(), key);
    }

    inline Result<void, AssetDatabaseError> set_name(const AssetName& name)
    {
        return db->set_asset_name(txn, guid, name);
    }

    inline Result<void, AssetDatabaseError> add_dependency(const GUID& child)
    {
        return db->add_dependency(txn, guid, child);
    }

    inline Result<AssetImportContext, AssetDatabaseError> create_sub_asset()
    {
        auto res = db->create_asset(txn);
        if (!res)
        {
            return res.unwrap_error();
        }

        auto* info = res.unwrap();
        info->kind = AssetFileKind::sub_asset;
        info->importer = importer_hash;

        const auto sub_asset = info->guid;
        auto sub_asset_res = db->set_sub_asset(txn, guid, sub_asset);
        if (!sub_asset_res)
        {
            return sub_asset_res.unwrap_error();
        }

        AssetImportContext ctx{};
        memcpy(&ctx, this, sizeof(AssetImportContext));
        ctx.guid = sub_asset;
        ctx.path = PathView{};
        return BEE_MOVE(ctx);
    }
};

struct AssetImporter
{
    const char* (*name)() { nullptr };

    i32 (*supported_file_types)(const char** dst) { nullptr };

    Type (*settings_type)() { nullptr };

    Result<void, AssetPipelineError> (*import)(AssetImportContext* ctx, void* user_data) { nullptr };
};


/*
 **********************************
 *
 * Asset runtime load API
 *
 **********************************
 */
#ifndef BEE_ASSET_LOCATION_MAX_STREAMS
    #define BEE_ASSET_LOCATION_MAX_STREAMS 32
#endif // BEE_ASSET_LOCATION_MAX_STREAMS


BEE_SPLIT_HANDLE(AssetHandle, u64, 32, 32, loader_id, asset_id);

template <typename T>
struct Asset final : public Noncopyable
{
    AssetPipelineModule*    module { nullptr };
    AssetPipeline*          pipeline { nullptr };
    AssetHandle             handle;
    T*                      data { nullptr };

    Asset() = default;

    Asset(Asset&& other) noexcept
        : module(other.module),
          pipeline(other.pipeline),
          handle(other.handle),
          data(other.data)
    {
        other.module = nullptr;
        other.pipeline = nullptr;
        other.handle = AssetHandle{};
        other.data = nullptr;
    }

    ~Asset();

    inline Asset& operator=(Asset&& other) noexcept
    {
        module = other.module;
        pipeline = other.pipeline;
        handle = other.handle;
        data = other.data;
        other.module = nullptr;
        other.pipeline = nullptr;
        other.handle = AssetHandle{};
        other.data = nullptr;
        return *this;
    }

    Result<i32, AssetPipelineError> unload();

    inline bool is_valid() const
    {
        return module != nullptr && pipeline != nullptr && handle.is_valid() && data != nullptr;
    }

    inline operator T*()
    {
        return data;
    }

    inline operator const T*() const
    {
        return data;
    }

    inline T* operator->()
    {
        return data;
    }

    inline const T* operator->() const
    {
        return data;
    }
};

struct AssetStreamInfo
{
    enum class Kind
    {
        none,
        file,
        buffer
    };

    Kind    kind { Kind::none };
    Path    path;
    u128    hash;
    u32     key { 0 };
    void*   buffer;
    size_t  offset { 0 };
    size_t  size { 0 };
};

struct AssetLocation
{
    Type                                                            type;
    StaticArray<AssetStreamInfo, BEE_ASSET_LOCATION_MAX_STREAMS>    streams;
};

struct AssetKey
{
    enum class Kind
    {
        none,
        guid,
        name
    };

    Kind kind { Kind::none };

    union
    {
        GUID        guid;
        AssetName   name;
    };

    AssetKey(const GUID& key)
        : kind(Kind::guid),
          guid(key)
    {}

    AssetKey(const AssetName& key)
        : kind(Kind::name),
          name(key)
    {}

    AssetKey(const StringView& name)
        : AssetKey(AssetName(name))
    {}

    AssetKey(const char* name)
        : AssetKey(AssetName(name))
    {}
};

template <>
struct Hash<AssetKey>
{
    inline u32 operator()(const AssetKey& key)
    {
        if (key.kind == AssetKey::Kind::guid)
        {
            return get_hash(key.guid);
        }

        if (key.kind == AssetKey::Kind::name)
        {
            return get_hash(key.name.data, key.name.size, 0);
        }

        return 0;
    }
};

struct AssetLocator
{
    void* user_data { nullptr };

    bool (*locate)(const AssetKey& key, const Type type, AssetLocation* location, void* user_data) { nullptr };
};

struct AssetLoader
{
    i32 (*get_types)(Type* dst) { nullptr };

    Result<void, AssetPipelineError> (*load)(const GUID guid, const AssetLocation* location, void* user_data, const AssetHandle handle, void* data) { nullptr };

    Result<void, AssetPipelineError> (*unload)(const Type type, void* data, void* user_data) { nullptr };

    void (*tick)(void* user_data) { nullptr };
};

/*
 **********************************
 *
 * Asset Pipeline module
 *
 **********************************
 */
#define BEE_ASSET_PIPELINE_MODULE_NAME "BEE_ASSET_PIPELINE"

BEE_FLAGS(AssetPipelineFlags, u8)
{
    none    = 0,
    import  = 1u << 0u,
    load    = 1u << 1u
};

struct AssetPipelineImportInfo
{
    StringView  name;
    PathView    cache_root;
    PathView*   source_roots { nullptr };
    i32         source_root_count { 0 };
};

struct AssetPipelineInfo
{
    AssetPipelineFlags              flags { AssetPipelineFlags::none };
    const AssetPipelineImportInfo*  import { nullptr };
};

struct AssetDatabase;
struct AssetPipeline;
struct AssetPipelineModule
{
    Result<AssetPipeline*, AssetPipelineError> (*create_pipeline)(const AssetPipelineInfo& info) { nullptr };

    void (*destroy_pipeline)(AssetPipeline* pipeline) { nullptr };

    AssetPipelineFlags (*get_flags)(const AssetPipeline* pipeline) { nullptr };

    Result<void, AssetPipelineError> (*register_importer)(AssetPipeline* pipeline, AssetImporter* importer, void* user_data) { nullptr };

    Result<void, AssetPipelineError> (*unregister_importer)(AssetPipeline* pipeline, AssetImporter* importer) { nullptr };

    Result<void, AssetPipelineError> (*register_loader)(AssetPipeline* pipeline, AssetLoader* loader, void* user_data) { nullptr };

    Result<void, AssetPipelineError> (*unregister_loader)(AssetPipeline* pipeline, AssetLoader* loader) { nullptr };

    Result<void, AssetPipelineError> (*register_locator)(AssetPipeline* pipeline, AssetLocator* locator) { nullptr };

    Result<void, AssetPipelineError> (*unregister_locator)(AssetPipeline* pipeline, AssetLocator* locator) { nullptr };

    Result<void, AssetPipelineError> (*refresh)(AssetPipeline* pipeline) { nullptr };

    /*
     ***************
     * Import API
     ***************
     */
    Result<void, AssetPipelineError> (*import_asset)(AssetPipeline* pipeline, const PathView& path, const AssetPlatform platform) { nullptr };

    Result<AssetDatabase*, AssetPipelineError> (*get_asset_database)(AssetPipeline* pipeline) { nullptr };

    void (*add_import_root)(AssetPipeline* pipeline, const PathView& path) { nullptr };

    void (*remove_import_root)(AssetPipeline* pipeline, const PathView& path) { nullptr };

    /*
     ***************
     * Load API
     ***************
     */
    Result<AssetHandle, AssetPipelineError> (*load_asset_from_key)(AssetPipeline* pipeline, const AssetKey& key, const Type type) { nullptr };

    Result<i32, AssetPipelineError> (*unload_asset)(AssetPipeline* pipeline, const AssetHandle handle) { nullptr };

    Result<void*, AssetPipelineError> (*get_asset_data)(AssetPipeline* pipeline, const AssetHandle handle) { nullptr };

    bool (*is_asset_loaded)(AssetPipeline* pipeline, const AssetKey& key) { nullptr };

    bool (*locate_asset)(AssetPipeline* pipeline, const AssetKey& key, const Type type, AssetLocation* location) { nullptr };

    template <typename T>
    inline Result<Asset<T>, AssetPipelineError> load_asset(AssetPipeline* pipeline, const AssetKey& key)
    {
        auto handle = load_asset_from_key(pipeline, key, get_type<T>());
        if (!handle)
        {
            return handle.unwrap_error();
        }
        auto data = get_asset_data(pipeline, handle.unwrap());
        if (!data)
        {
            return data.unwrap_error();
        }
        Asset<T> asset;
        asset.data = static_cast<T*>(data.unwrap());
        asset.handle = handle.unwrap();
        asset.pipeline = pipeline;
        asset.module = this;

        return BEE_MOVE(asset);
    }
};

template <typename T>
Asset<T>::~Asset<T>()
{
    unload();
}

template <typename T>
Result<i32, AssetPipelineError> Asset<T>::unload()
{
    if (!is_valid())
    {
        return { AssetPipelineError::invalid_data };
    }
    return module->unload_asset(pipeline, handle);
}


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.AssetPipeline/AssetPipeline.generated.inl"
#endif // BEE_ENABLE_REFLECTION