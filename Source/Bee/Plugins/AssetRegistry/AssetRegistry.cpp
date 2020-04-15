/*
 *  AssetRegistry.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/Plugins/AssetRegistry/AssetRegistry.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Containers/ResourcePool.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"


namespace bee {


struct AssetCache
{
    RecursiveSpinLock                   mutex;
    ResourcePool<AssetId, AssetData>    cache;
    DynamicHashMap<GUID, AssetId>       guid_to_id;

    AssetCache() noexcept
        : cache(sizeof(AssetData) * 64)
    {}

    AssetData* get(const AssetId& id)
    {
        scoped_recursive_spinlock_t lock(mutex);

        if (!cache.is_active(id))
        {
            return nullptr;
        }

        return &cache[id];
    }

    AssetData* find(const GUID& guid)
    {
        scoped_recursive_spinlock_t lock(mutex);

        auto* existing_data = guid_to_id.find(guid);

        if (existing_data != nullptr)
        {
            return &cache[existing_data->value];
        }

        return nullptr;
    }

    AssetData* insert(const GUID& guid)
    {
        scoped_recursive_spinlock_t lock(mutex);

        BEE_ASSERT(guid_to_id.find(guid) == nullptr);

        const auto id = cache.allocate();
        auto* data = &cache[id];
        data->id = id;
        data->status = AssetStatus::unloaded;
        data->type = get_type<UnknownType>();
        data->guid = guid;
        data->ptr = nullptr;

        guid_to_id.insert(guid, id);

        return data;
    }

    void erase(const AssetId& id)
    {
        scoped_recursive_spinlock_t lock(mutex);

        auto& asset = cache[id];
        guid_to_id.erase(asset.guid);
        cache.deallocate(id);
    }

    void clear()
    {
        scoped_recursive_spinlock_t lock(mutex);

        for (auto& asset : cache)
        {
            if (asset.loader != nullptr)
            {
                AssetLoaderContext ctx(&asset);
                asset.loader->unload(&ctx);
            }
        }

        cache.clear();
        guid_to_id.clear();
    }
};

struct RegisteredLoader
{
    AssetLoaderApi*             api { nullptr };
    DynamicArray<const Type*>   supported_types;
};


static JobGroup                                 g_load_jobs;
static AssetCache                               g_cache;
static DynamicHashMap<u32, AssetLoaderApi*>     g_type_hash_to_loader;
static DynamicArray<RegisteredLoader>           g_loaders;
static DynamicArray<AssetLocatorApi*>           g_locators;
static AssetRegistryApi                         g_api;


void add_loader(AssetLoaderApi* loader);
void remove_loader(AssetLoaderApi* loader);
void add_locator(AssetLocatorApi* locator);
void remove_locator(AssetLocatorApi* locator);
bool locate_asset(const GUID& guid, AssetLocation* location);
void unload_asset(const AssetData* asset, const UnloadAssetMode kind);


void asset_registry_destroy()
{
    job_wait(&g_load_jobs);
    g_cache.clear();
}

void loader_observer(const PluginEventType event, const char* plugin_name, void* interface, void* user_data)
{
    auto* registry = static_cast<AssetRegistryApi*>(user_data);
    auto* loader = static_cast<AssetLoaderApi*>(interface);

    if (event == PluginEventType::add_interface)
    {
        registry->add_loader(loader);
    }

    if (event == PluginEventType::remove_interface)
    {
        registry->remove_loader(loader);
    }
}

void locator_observer(const PluginEventType event, const char* plugin_name, void* interface, void* user_data)
{
    auto* registry = static_cast<AssetRegistryApi*>(user_data);
    auto* locator = static_cast<AssetLocatorApi*>(interface);

    if (event == PluginEventType::add_interface)
    {
        registry->add_locator(locator);
    }

    if (event == PluginEventType::remove_interface)
    {
        registry->remove_locator(locator);
    }
}

void load_asset_job(AssetData* asset, AssetLoaderApi* loader)
{
    AssetLocation location{};

    if (!locate_asset(asset->guid, &location))
    {
        log_error("Failed to find a location for asset %s", format_guid(asset->guid, GUIDFormat::digits));
        return;
    }

    if (location.type != asset->type)
    {
        log_error(
            "Located asset %s at %s but the located type `%s` doesn't match the expected type `%s`",
            format_guid(asset->guid, GUIDFormat::digits),
            location.path.c_str(),
            location.type->name,
            asset->type->name
        );

        return;
    }

    io::FileStream stream(location.path, "rb");
    stream.seek(location.offset, io::SeekOrigin::begin);

    AssetLoaderContext ctx(asset);
    asset->status = loader->load(&ctx, &stream);

    if (asset->status == AssetStatus::loaded)
    {
        ++asset->refcount;
    }
}

AssetData* get_or_load_asset(const GUID& guid, const Type* type)
{
    auto* cached = g_cache.find(guid);

    // Check if the requested type is the same as the type that loaded successfully previously
    if (cached != nullptr && cached->type != type)
    {
        log_error(
            "Invalid asset type given for asset %s: requested type %s but expected type %s",
            format_guid(guid, GUIDFormat::digits),
            type->name,
            cached->type->name
        );

        return nullptr;
    }

    // Try and find a loader for the requested type
    auto* loader = g_type_hash_to_loader.find(type->hash);

    if (loader == nullptr)
    {
        log_error("Failed to find a registered loader that can handle assets of type %s", type->name);
        return nullptr;
    }

    // Now that we know we have a valid loader for the type we can add a new cache entry if needed
    if (cached == nullptr)
    {
        cached = g_cache.insert(guid);
    }

    cached->loader = loader->value;

    // Don't try to reload assets currently in flight or already loaded - add a reference instead
    if (cached->status == AssetStatus::loaded || cached->status == AssetStatus::loading)
    {
        ++cached->refcount;
        return cached;
    }

    // Allocate new data if we're not reloading
    if (cached->ptr == nullptr)
    {
        cached->ptr = cached->loader->allocate(type);
    }

    auto* job = create_job(load_asset_job, cached, cached->loader);
    job_schedule(&g_load_jobs, job);

    return cached;
}

void unload_asset(AssetData* asset, const UnloadAssetMode kind)
{
    if (g_cache.get(asset->id) == nullptr)
    {
        log_error("No such asset with id %" PRIu64, asset->id.id);
        return;
    }

    // Try and just release the reference
    auto expected = 1;
    if (kind == UnloadAssetMode::release && !asset->refcount.compare_exchange_strong(expected, 0, std::memory_order_seq_cst))
    {
        --asset->refcount;
        return;
    }

    // unload and deallocate the asset if last ref or otherwise explicitly requested
    AssetLoaderContext ctx(asset);
    asset->status = asset->loader->unload(&ctx);

    if (asset->status == AssetStatus::unloaded)
    {
        g_cache.erase(asset->id);
    }
}

bool locate_asset(const GUID& guid, AssetLocation* location)
{
    for (auto& locator : g_locators)
    {
        if (locator->locate(guid, location))
        {
            return true;
        }
    }

    return false;
}

void add_loader(AssetLoaderApi* loader)
{
    job_wait(&g_load_jobs);

    const auto existing_index = container_index_of(g_loaders, [&](const RegisteredLoader& l)
    {
        return l.api == loader;
    });

    if (existing_index >= 0)
    {
        log_error("Asset loader was added multiple times to the asset registry");
        return;
    }

    g_loaders.emplace_back();

    auto& registered = g_loaders.back();
    registered.api = loader;
    registered.api->get_supported_types(&registered.supported_types);

    // Add mappings for all the supported types to the loader API
    for (const auto& type : registered.supported_types)
    {
        auto* type_mapping = g_type_hash_to_loader.find(type->hash);

        if (type_mapping != nullptr)
        {
            log_error("A loader is already registered to handle type %s", type->name);
            continue;
        }

        g_type_hash_to_loader.insert(type->hash, registered.api);
    }
}

void remove_loader(AssetLoaderApi* loader)
{
    job_wait(&g_load_jobs);

    const auto index = container_index_of(g_loaders, [&](const RegisteredLoader& l)
    {
        return l.api == loader;
    });

    if (index < 0)
    {
        log_error("Asset loader was not previously added to the Asset Registry");
        return;
    }

    for (const auto& type : g_loaders[index].supported_types)
    {
        g_type_hash_to_loader.erase(type->hash);
    }

    g_loaders.erase(index);
}

void add_locator(AssetLocatorApi* locator)
{
    job_wait(&g_load_jobs);

    const auto existing_index = container_index_of(g_locators, [&](const AssetLocatorApi* l)
    {
        return l == locator;
    });

    if (existing_index >= 0)
    {
        log_error("Asset locator was added multiple times to the asset registry");
        return;
    }

    g_locators.push_back(locator);
}

void remove_locator(AssetLocatorApi* locator)
{
    job_wait(&g_load_jobs);

    const auto index = container_index_of(g_locators, [&](const AssetLocatorApi* l)
    {
        return l == locator;
    });

    if (index < 0)
    {
        log_error("Asset locator was not previously added to the Asset Registry");
        return;
    }

    g_locators.erase(index);
}


} // namespace bee


BEE_PLUGIN_API void load_plugin(bee::PluginRegistry* registry)
{
    bee::g_api.load_asset_data = bee::get_or_load_asset;
    bee::g_api.unload_asset_data = bee::unload_asset;

    add_plugin_observer(BEE_ASSET_LOADER_API_NAME, bee::loader_observer);
    add_plugin_observer(BEE_ASSET_LOCATOR_API_NAME, bee::locator_observer);
}

BEE_PLUGIN_API void unload_plugin(bee::PluginRegistry* registry)
{
    remove_plugin_observer(BEE_ASSET_LOADER_API_NAME, bee::loader_observer);
    remove_plugin_observer(BEE_ASSET_LOCATOR_API_NAME, bee::locator_observer);

    bee::asset_registry_destroy();
}