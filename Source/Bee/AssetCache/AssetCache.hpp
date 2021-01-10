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
    enum Enum
    {
        unknown,
        failed_to_locate,
        no_loader_for_type,
        failed_to_load,
        failed_to_unload,
        invalid_asset_handle,
        missing_data,
        invalid_data,
        count
    };

    BEE_ENUM_STRUCT(AssetCacheError);

    const char* to_string() const
    {
        BEE_TRANSLATION_TABLE(value, Enum, const char*, Enum::count,
            "Unknown Asset Cache error",                                        // unknown
            "Failed to locate asset from GUID",                                 // failed_to_locate
            "Unable to find a loader registered for the located asset type",    // no_loader_for_type
            "Failed to load asset data",                                        // failed_to_load
            "Failed to unload asset data",                                      // failed_to_unload
            "Invalid asset handle",                                             // invalid_asset_handle
            "Content hash for asset resolved to missing data",                  // missing_data
            "Asset data has an invalid format or is corrupted",                 // invalid_data
        )
    }
};

struct AssetStreamInfo
{
    AssetStreamKind kind { AssetStreamKind::none };
    Path            path;
    u128            hash;
    void*           buffer;
    size_t          offset { 0 };
    size_t          size { 0 };
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

    Result<void*, AssetCacheError> (*load)(const AssetLocation* location, void* user_data) { nullptr };

    Result<void, AssetCacheError> (*unload)(const Type type, void* data, void* user_data) { nullptr };
};

#define BEE_ASSET_CACHE_MODULE_NAME "BEE_ASSET_CACHE"

struct AssetCache;
struct AssetCacheModule
{
    AssetCache* (*create_cache)() { nullptr };

    void (*destroy_cache)(AssetCache* cache) { nullptr };

    bool (*register_loader)(AssetCache* cache, AssetLoader* loader, void* user_data) { nullptr };

    void (*unregister_loader)(AssetCache* cache, AssetLoader* loader) { nullptr };

    void (*register_locator)(AssetCache* cache, AssetLocator* locator) { nullptr };

    void (*unregister_locator)(AssetCache* cache, AssetLocator* locator) { nullptr };

    Result<AssetHandle, AssetCacheError> (*load_asset)(AssetCache* cache, const GUID guid) { nullptr };

    bool (*is_asset_loaded)(AssetCache* cache, const GUID guid) { nullptr };

    Result<i32, AssetCacheError> (*unload_asset)(AssetCache* cache, const AssetHandle handle) { nullptr };

    Result<void*, AssetCacheError> (*get_asset_data)(AssetCache* cache, const AssetHandle handle) { nullptr };

    template <typename T>
    Result<T*, AssetCacheError> get_asset(AssetCache* cache, const AssetHandle handle)
    {
        auto data = get_asset_data(cache, handle);
        if (!data)
        {
            return data.unwrap_error();
        }
        return static_cast<T*>(data.unwrap());
    }
};


} // namespace bee