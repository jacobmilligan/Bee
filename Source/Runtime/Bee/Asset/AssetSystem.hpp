/*
 *  AssetSystem.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Asset/Asset.hpp"


namespace bee {


struct AssetLoadRequest
{
    GUID            asset_guid;
    AssetLoadMode   mode;
};


struct BEE_RUNTIME_API AssetLoader
{
    virtual ~AssetLoader() = default;

    virtual void setup(DynamicArray<Type>* context) = 0;

    virtual bool load_asset(const AssetLoadMode mode, AssetPtr& asset, io::Stream* src_stream) = 0;

    virtual void unload_asset(const AssetUnloadMode mode, AssetPtr& asset) = 0;
};


struct BEE_RUNTIME_API AssetRegistry
{
    virtual ~AssetRegistry() = default;

    virtual bool locate_asset(const GUID& guid, io::FileStream* dst_stream) = 0;
};


BEE_RUNTIME_API void register_asset_loader(AssetLoader* loader);

BEE_RUNTIME_API void unregister_asset_loader(const AssetLoader* loader);

BEE_RUNTIME_API void add_asset_registry(AssetRegistry* registry);

BEE_RUNTIME_API void remove_asset_registry(const AssetRegistry* registry);

BEE_RUNTIME_API void register_asset_cache(AssetCache* cache);

BEE_RUNTIME_API void unregister_asset_cache(const AssetCache* cache);

BEE_RUNTIME_API void load_assets(JobGroup* group, const AssetLoadRequest* load_requests, AssetData* dst_assets, const i32 count);

BEE_RUNTIME_API void load_assets_sync(const AssetLoadRequest* load_requests, AssetData* dst_assets, const i32 count);

BEE_RUNTIME_API void unload_asset(AssetData* asset, const AssetUnloadMode mode);

template <typename AssetType>
void load_asset(JobGroup* group, const GUID& guid, const AssetLoadMode mode, Asset<AssetType>* dst_asset)
{
    AssetLoadRequest req{};
    req.asset_guid = guid;
    req.mode = mode;

    load_assets(group, &req, dst_asset, 1);
}

template <typename AssetType>
Asset<AssetType> load_asset(const GUID& guid, const AssetLoadMode mode = AssetLoadMode::load)
{
    Asset<AssetType> asset;
    AssetLoadRequest req{};
    req.asset_guid = guid;
    req.mode = mode;

    load_assets_sync(&req, &asset, 1);

    return asset;
}


} // namespace bee