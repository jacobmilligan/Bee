/*
 *  AssetLoader.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/GUID.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/Containers/StaticArray.hpp"
#include "Bee/Core/Result.hpp"


namespace bee {


#ifndef BEE_ASSET_LOCATION_MAX_STREAMS
    #define BEE_ASSET_LOCATION_MAX_STREAMS 32
#endif // BEE_ASSET_LOCATION_MAX_STREAMS


BEE_VERSIONED_HANDLE_64(AssetHandle);


enum class AssetStreamKind
{
    none,
    file,
    buffer
};

struct AssetCacheError
{
    enum Status
    {
        unknown,
        failed_to_locate,
        no_loader_for_type,
        failed_to_load,
        failed_to_unload,
        count
    };

    Status      status { unknown };

    const char* to_string() const
    {
        BEE_TRANSLATION_TABLE(status, Status, const char*, Status::count,
            "Unknown Asset Cache error",                                        // unknown
            "Failed to locate asset from GUID",                                 // failed_to_locate
            "Unable to find a loader registered for the located asset type",    // no_loader_for_type
            "Failed to load asset data",                                        // failed_to_load
            "Failed to unload asset data",                                      // failed_to_unload
        )
    }
};

struct AssetStreamInfo
{
    AssetStreamKind kind { AssetStreamKind::none };
    Path            path;
    void*           buffer;
    size_t          offset { 0 };
};

struct AssetLocation
{
    Type                                                            type;
    StaticArray<AssetStreamInfo, BEE_ASSET_LOCATION_MAX_STREAMS>    streams;
};

struct AssetLocator
{
    void* user_data { nullptr };

    bool (*locate)(void* user_data, const GUID guid, AssetLocation* location) { nullptr };
};

struct AssetLoader
{
    i32 (*get_types)(Type* dst) { nullptr };

    Result<void*, AssetCacheError> (*load)(const AssetLocation* location) { nullptr };

    bool (*unload)(const Type type, void* data) { nullptr };
};

#define BEE_ASSET_CACHE_MODULE_NAME "BEE_ASSET_CACHE"

struct AssetCache;
struct AssetCacheModule
{
    AssetCache* (*create_cache)() { nullptr };

    void (*destroy_cache)(AssetCache* cache) { nullptr };

    bool (*register_loader)(AssetCache* cache, AssetLoader* loader) { nullptr };

    void (*unregister_loader)(AssetCache* cache, AssetLoader* loader) { nullptr };

    void (*register_locator)(AssetCache* cache, AssetLocator* locator) { nullptr };

    void (*unregister_locator)(AssetCache* cache, AssetLocator* locator) { nullptr };

    Result<AssetHandle, AssetCacheError> (*load_asset)(AssetCache* cache, const GUID guid) { nullptr };

    Result<i32, AssetCacheError> (*unload_asset)(AssetCache* cache, const AssetHandle handle) { nullptr };
};


} // namespace bee