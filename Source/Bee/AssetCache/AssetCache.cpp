/*
 *  AssetLoader.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Memory/Memory.hpp"
#include "Bee/AssetCache/AssetCache.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Memory/PoolAllocator.hpp"
#include "Bee/Core/Containers/ResourcePool.hpp"
#include "Bee/Core/Atomic.hpp"


namespace bee {


struct AssetLoaderInfo
{
    FixedArray<Type>    types;
    AssetLoader*        loader { nullptr };
};

struct AssetInfo
{
    i32             refcount { 0 };
    GUID            guid;
    AssetLocation   location;
    AssetLoader*    loader { nullptr };
    void*           data { nullptr };
};

struct AssetCache
{
    DynamicArray<AssetLocator*>             locators;
    DynamicArray<AssetLoaderInfo>           loaders;
    DynamicHashMap<Type, AssetLoader*>      type_to_loader;

    DynamicHashMap<GUID, AssetHandle>       lookup;
    ResourcePool<AssetHandle, AssetInfo>    assets { sizeof(AssetInfo) * 64 };
};

AssetCache* create_cache()
{
    return BEE_NEW(system_allocator(), AssetCache);
}

void destroy_cache(AssetCache* cache)
{
    BEE_DELETE(system_allocator(), cache);
}

bool register_loader(AssetCache* cache, AssetLoader* loader)
{
    cache->loaders.emplace_back();
    auto& info = cache->loaders.back();
    info.loader = loader;

    const int type_count = loader->get_types(nullptr);
    BEE_ASSERT(type_count > 0);

    info.types.resize(type_count);
    loader->get_types(info.types.data());

    // Validate the supported types are not already registered to a different loader
    bool valid = true;
    for (const auto& type : info.types)
    {
        if (cache->type_to_loader.find(type) != nullptr)
        {
            valid = false;
            break;
        }
    }

    if (!valid)
    {
        cache->loaders.pop_back();
        return false;
    }

    for (const auto& type : info.types)
    {
        cache->type_to_loader.insert(type, loader);
    }

    return true;
}

void unregister_loader(AssetCache* cache, AssetLoader* loader)
{
    const int index = find_index_if(cache->loaders, [&](const AssetLoaderInfo& info)
    {
        return info.loader == loader;
    });

    if (BEE_FAIL_F(index >= 0, "AssetLoader is not registered"))
    {
        return;
    }

    for (const auto& type : cache->loaders[index].types)
    {
        cache->type_to_loader.erase(type);
    }

    cache->loaders.erase(index);
}

void register_locator(AssetCache* cache, AssetLocator* locator)
{
    const int index = find_index(cache->locators, locator);
    if (index < 0)
    {
        cache->locators.push_back(locator);
    }
}

void unregister_locator(AssetCache* cache, AssetLocator* locator)
{
    const int index = find_index(cache->locators, locator);
    if (index >= 0)
    {
        cache->locators.erase(index);
    }
}

Result<AssetHandle, AssetCacheError> load_asset(AssetCache* cache, const GUID guid)
{
    auto* existing_asset = cache->lookup.find(guid);

    if (existing_asset != nullptr)
    {
        ++cache->assets[existing_asset->value].refcount;
        return existing_asset->value;
    }

    const auto handle = cache->assets.allocate();
    auto& new_asset = cache->assets[handle];

    const int locator = find_index_if(cache->locators, [&](AssetLocator* locator)
    {
        return locator->locate(locator->user_data, guid, &new_asset.location);
    });

    if (locator < 0)
    {
        cache->assets.deallocate(handle);
        return AssetCacheError { AssetCacheError::Status::failed_to_locate };
    }

    if (cache->type_to_loader.find(new_asset.location.type) == nullptr)
    {
        cache->assets.deallocate(handle);
        return AssetCacheError { AssetCacheError::Status::no_loader_for_type };
    }


    auto& asset = cache->assets[handle];
    asset.guid = guid;
    auto* loader_info = cache->type_to_loader.find(asset.location.type);

    if (loader_info == nullptr)
    {
        cache->assets.deallocate(handle);
        return AssetCacheError { AssetCacheError::no_loader_for_type };
    }

    auto res = loader_info->value->load(&asset.location);
    if (!res)
    {
        cache->assets.deallocate(handle);
        return res.unwrap_error();
    }

    ++asset.refcount;
    asset.loader = loader_info->value;
    asset.data = res.unwrap();
    return handle;
}

Result<i32, AssetCacheError> unload_asset(AssetCache* cache, const AssetHandle handle)
{
    auto& asset = cache->assets[handle];
    --asset.refcount;
    if (asset.refcount > 0)
    {
        return asset.refcount;
    }

    if (!asset.loader->unload(asset.location.type, asset.data))
    {
        return AssetCacheError::failed_to_unload;
    }

    cache->lookup.erase(asset.guid);
    cache->assets.deallocate(handle);
    return 0;
}


} // namespace bee

static bee::AssetCacheModule g_module{};

BEE_PLUGIN_API void bee_load_plugin(bee::PluginLoader* loader, const bee::PluginState state)
{
    g_module.create_cache = bee::create_cache;
    g_module.destroy_cache = bee::destroy_cache;
    g_module.register_loader = bee::register_loader;
    g_module.unregister_loader = bee::unregister_loader;
    g_module.register_locator = bee::register_locator;
    g_module.unregister_locator = bee::unregister_locator;
    g_module.load_asset = bee::load_asset;

    loader->set_module(BEE_ASSET_CACHE_MODULE_NAME, &g_module, state);
}

BEE_PLUGIN_VERSION(0, 0, 0)