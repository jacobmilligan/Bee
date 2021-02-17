/*
 *  AssetPipeline.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/String.hpp"
#include "Bee/Core/GUID.hpp"
#include "Bee/Core/Serialization/BinarySerializer.hpp"

#include "Bee/AssetDatabase/AssetDatabase.hpp"


namespace bee {


enum class ImportErrorStatus
{
    success,
    unsupported_file_type,
    failed_to_write_metadata,
    fatal
};

BEE_REFLECTED_FLAGS(AssetPlatform, u32, serializable)
{
    unknown = 0u,
    windows = 1u << 0u,
    macos   = 1u << 1u,
    linux   = 1u << 2u,
    metal   = 1u << 3u,
    vulkan  = 1u << 4u,
};


struct AssetImportContext
{
    Allocator*              temp_allocator { nullptr };
    AssetPlatform           platform { AssetPlatform::unknown };
    const AssetMetadata*    metadata { nullptr };
    AssetDatabaseModule*    db { nullptr };
    AssetTxn*               txn { nullptr };
    DynamicArray<u8>*       artifact_buffer { nullptr };
    StringView              path;

    template <typename T>
    const T* get_properties() const
    {
        BEE_ASSERT(metadata != nullptr);
        return metadata->properties.get<T>();
    }

    template <typename T>
    inline Result<u128, AssetDatabaseError> add_artifact(const T* artifact)
    {
        BinarySerializer serializer(artifact_buffer);
        serialize(SerializerMode::writing, &serializer, const_cast<T*>(artifact), temp_allocator);
        return db->add_artifact(txn, metadata->guid, get_type<T>(), artifact_buffer->data(), artifact_buffer->size());
    }

    inline Result<u128, AssetDatabaseError> add_artifact(const Type type, const size_t buffer_size, const void* buffer)
    {
        return db->add_artifact(txn, metadata->guid, type, artifact_buffer->data(), artifact_buffer->size());
    }
};

struct AssetImporter
{
    const char* (*name)() { nullptr };

    i32 (*supported_file_types)(const char** dst) { nullptr };

    Type (*properties_type)() { nullptr };

    ImportErrorStatus (*import)(AssetImportContext* ctx, void* user_data) { nullptr };
};


#define BEE_ASSET_PIPELINE_MODULE_NAME "BEE_ASSET_PIPELINE"

struct BEE_REFLECT(serializable) AssetPipelineConfig
{
    String              name;
    Path                cache_root;
    DynamicArray<Path>  source_roots;
};

struct AssetPipelineInfo
{
    StringView  name;
    StringView  config_path;
    StringView  cache_root;
    StringView* source_roots { nullptr };
    i32         source_root_count { 0 };
};

struct AssetCache;
struct AssetImportPipeline;
struct AssetImportPipelineModule
{
    AssetImportPipeline* (*create_pipeline)(const AssetPipelineInfo& info) { nullptr };

    AssetImportPipeline* (*load_pipeline)(const StringView path) { nullptr };

    void (*destroy_pipeline)(AssetImportPipeline* pipeline) { nullptr };

    AssetDatabase* (*get_asset_db)(AssetImportPipeline* pipeline) { nullptr };

    void (*add_source_folder)(AssetImportPipeline* pipeline, const Path& path) { nullptr };

    void (*remove_source_folder)(AssetImportPipeline* pipeline, const Path& path) { nullptr };

    void (*watch_external_sources)(const Path& folder, const bool watch) { nullptr };

    void (*register_importer)(AssetImportPipeline* pipeline, AssetImporter* importer, void* user_data) { nullptr };

    void (*unregister_importer)(AssetImportPipeline* pipeline, AssetImporter* importer) { nullptr };

    ImportErrorStatus (*import_asset)(AssetImportPipeline* pipeline, const StringView path, const AssetPlatform platform) { nullptr };

    void (*refresh)(AssetImportPipeline* pipeline) { nullptr };

    void (*set_runtime_cache)(AssetImportPipeline* pipeline, AssetCache* cache) { nullptr };
};


} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.AssetPipeline/Import.generated.inl"
#endif // BEE_ENABLE_REFLECTION