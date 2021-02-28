/*
 *  AssetPipeline.inl
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/AssetPipeline/AssetPipeline.hpp"

#include "Bee/Core/Path.hpp"
#include "Bee/Core/Filesystem.hpp"
#include "Bee/Core/Containers/ResourcePool.hpp"
#include "Bee/Core/Atomic.hpp"


namespace bee {


/*
 **********************************
 *
 * Asset import pipeline
 *
 **********************************
 */
struct FileTypeInfo
{
    const char*         extension { nullptr };
    DynamicArray<u32>   importer_hashes;
};

struct ImporterInfo
{
    AssetImporter*      importer { nullptr };
    void*               user_data { nullptr };
    DynamicArray<u32>   file_type_hashes;
};

struct ImportPipeline
{
    String                              name;
    Path                                cache_path;
    Path                                db_path;
    AssetDatabase*                      db { nullptr };
    fs::DirectoryWatcher                source_watcher;
    DynamicArray<fs::FileNotifyInfo>    source_events;
    AssetLocator                        asset_database_locator;

    // Importer data
    DynamicArray<u32>                   file_type_hashes;
    DynamicArray<FileTypeInfo>          file_types;
    DynamicArray<u32>                   importer_hashes;
    DynamicArray<ImporterInfo>          importers;
};

/*
 **********************************
 *
 * Asset load pipeline
 *
 **********************************
 */

struct LoaderId
{
    BEE_VERSIONED_HANDLE_BODY(LoaderId, bee::u32, 24u, 8u)

    explicit LoaderId(const AssetHandle& handle)
        : id(sign_cast<u32>(handle.loader_id()))
    {}
};

struct AssetId
{
    BEE_VERSIONED_HANDLE_BODY(AssetId, bee::u32, 24u, 8u)

    explicit AssetId(const AssetHandle& handle)
        : id(sign_cast<u32>(handle.asset_id()))
    {}
};

struct LoadedAsset
{
    atomic_i32      refcount { 0 };
    GUID            guid;
    AssetLocation   location;
    TypeInstance    data;
    LoaderId        loader;
};

struct Loader
{
    AssetLoader*                        instance { nullptr };
    void*                               user_data { nullptr };
    FixedArray<Type>                    types;
    ResourcePool<AssetId, LoadedAsset>  assets;
    RecursiveMutex                      mutex;

    Loader()
        : assets(sizeof(LoadedAsset) * 64)
    {}
};

struct LoadPipeline
{
    DynamicArray<AssetLocator*>         locators;
    ResourcePool<LoaderId, Loader>      loaders;
    DynamicHashMap<Type, LoaderId>      type_to_loader;
    DynamicHashMap<u32, AssetHandle>    cache;
    RecursiveMutex                      cache_mutex;
    RecursiveMutex                      name_to_guid_mutex;

    LoadPipeline()
        : loaders(sizeof(Loader) * 16)
    {}
};

/*
 **********************************
 *
 * Asset pipeline
 *
 **********************************
 */
struct AssetPipeline
{
    struct ThreadData
    {
        DynamicArray<u8>            artifact_buffer;
        DynamicArray<AssetHandle>   pending_unloads;
        Path                        meta_path;
        Path                        source_path;
        String                      target_platform_string;
        DynamicArray<u8>            settings_buffer;
    };

    AssetPipelineFlags      flags { AssetPipelineFlags::none };
    ImportPipeline          import;
    LoadPipeline            load;
    FixedArray<ThreadData>  thread_data;

    bool can_import() const;

    bool can_load() const;

    ThreadData& get_thread();
};


} // namespace bee