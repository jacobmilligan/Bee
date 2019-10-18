/*
 *  AssetSystem.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Asset/AssetSystem.hpp"

#define BEE_ASSERT_NO_INFLIGHT_OPERATIONS() BEE_ASSERT_F(loads_in_flight_ <= 0, "Modifying the asset system while load or unload operations are in-flight is unsafe")

namespace bee {


struct LoaderInfo
{
    AssetLoader*        loader { nullptr };
    DynamicArray<Type>  supported_types;

    explicit LoaderInfo(AssetLoader* new_loader)
        : loader(new_loader)
    {}
};

struct CacheInfo
{
    ReaderWriterMutex   mutex;
    AssetCache*         cache { nullptr };
    DynamicArray<Type>  supported_types;

    explicit CacheInfo(AssetCache* new_cache)
        : cache(new_cache)
    {}
};

struct LoadAssetJob final : public Job
{
    FixedArray<AssetLoadRequest>    requests;
    AssetData*                      assets { nullptr };

    LoadAssetJob(const AssetLoadRequest* load_requests, AssetData* dst_assets, const i32 request_count, bool sync);

    void execute() override;
};


struct BEE_RUNTIME_API AssetSystem
{
    i32                                 loads_in_flight_ { 0 };
    i32                                 unloads_in_flight_ { 0 };
    DynamicArray<LoaderInfo>            loader_infos_;
    DynamicArray<AssetRegistry*>        registry_infos_;
    DynamicArray<CacheInfo>             cache_infos_;

    SpinLock                            asset_map_mutex_;
    DynamicHashMap<GUID, AssetData>     asset_map_;
    DynamicHashMap<Type, i32>           cache_map_;
    DefaultAssetCache                   fallback_cache_;

    AssetSystem();

    void register_loader(AssetLoader* loader);

    void unregister_loader(const AssetLoader* loader);

    void add_registry(AssetRegistry* registry);

    void remove_registry(const AssetRegistry* registry);

    void register_cache(AssetCache* cache);

    void unregister_cache(const AssetCache* cache);

    void load_assets(JobGroup* group, const AssetLoadRequest* load_requests, AssetData* dst_assets, const i32 count, bool sync);

    void unload_asset(AssetData* asset, const AssetUnloadMode mode);

    inline i32 find_loader_no_lock(const AssetLoader* loader)
    {
        return container_index_of(loader_infos_, [&](const LoaderInfo& l) { return l.loader == loader; } );
    }

    inline i32 find_registry_no_lock(const AssetRegistry* registry)
    {
        return container_index_of(registry_infos_, [&](const AssetRegistry* r) { return r == registry; } );
    }

    inline i32 find_cache_no_lock(const AssetCache* cache)
    {
        return container_index_of(cache_infos_, [&](const CacheInfo& c) { return c.cache == cache; });
    }

    bool locate_asset_no_lock(const GUID& guid, io::FileStream* stream);

    AssetData* find_cached_asset(const GUID& guid);

    i32 find_cache_for_type_no_lock(const Type& type);

    i32 find_loader_for_type_no_lock(const Type& type);
};


static AssetSystem g_asset_system;


void register_asset_loader(AssetLoader* loader)
{
    g_asset_system.register_loader(loader);
}

void unregister_asset_loader(const AssetLoader* loader)
{
    g_asset_system.unregister_loader(loader);
}

void add_asset_registry(AssetRegistry* registry)
{
    g_asset_system.add_registry(registry);
}

void remove_asset_registry(const AssetRegistry* registry)
{
    g_asset_system.remove_registry(registry);
}

void register_asset_cache(AssetCache* cache)
{
    g_asset_system.register_cache(cache);
}

void unregister_asset_cache(const AssetCache* cache)
{
    g_asset_system.unregister_cache(cache);
}

void load_assets(JobGroup* group, const AssetLoadRequest* load_requests, AssetData* dst_assets, const i32 count)
{
    g_asset_system.load_assets(group, load_requests, dst_assets, count, false);
}

void load_assets_sync(const AssetLoadRequest* load_requests, AssetData* dst_assets, const i32 count)
{
    g_asset_system.load_assets(nullptr, load_requests, dst_assets, count, true);
}

void unload_asset(AssetData* asset, const AssetUnloadMode mode)
{
    g_asset_system.unload_asset(asset, mode);
}


AssetSystem::AssetSystem()
{
    register_cache(&fallback_cache_);
}

void AssetSystem::register_loader(AssetLoader* loader)
{
    BEE_ASSERT_NO_INFLIGHT_OPERATIONS();

    if (BEE_FAIL_F(find_loader_no_lock(loader) < 0, "Loader is already registered"))
    {
        return;
    }

    loader_infos_.emplace_back(loader);
    loader->setup(&loader_infos_.back().supported_types);
}

void AssetSystem::unregister_loader(const AssetLoader* loader)
{
    BEE_ASSERT_NO_INFLIGHT_OPERATIONS();

    const auto index = find_loader_no_lock(loader);
    if (BEE_FAIL_F(index >= 0, "Loader is not registered"))
    {
        return;
    }

    loader_infos_.erase(index);
}

void AssetSystem::add_registry(AssetRegistry* registry)
{
    BEE_ASSERT_NO_INFLIGHT_OPERATIONS();

    if (BEE_FAIL_F(find_registry_no_lock(registry) < 0, "Registry is already added"))
    {
        return;
    }

    registry_infos_.emplace_back(registry);
}

void AssetSystem::remove_registry(const AssetRegistry* registry)
{
    BEE_ASSERT_NO_INFLIGHT_OPERATIONS();

    const auto index = find_registry_no_lock(registry);
    if (BEE_FAIL_F(index >= 0, "Registry is not registered"))
    {
        return;
    }

    registry_infos_.erase(index);
}

void AssetSystem::register_cache(AssetCache* cache)
{
    BEE_ASSERT_NO_INFLIGHT_OPERATIONS();

    const auto index = find_cache_no_lock(cache);
    if (BEE_FAIL_F(index < 0, "Cache is already registered"))
    {
        return;
    }

    cache_infos_.emplace_back(cache);
    cache->setup(&cache_infos_.back().supported_types);
    for (auto& type : cache_infos_.back().supported_types)
    {
        auto existing = cache_map_.find(type);
        if (BEE_FAIL_F(existing == nullptr, "A cache is already registered for type \"%s\"", type.name))
        {
            continue;
        }

        cache_map_.insert(type, cache_infos_.size() - 1);
    }
}

void AssetSystem::unregister_cache(const AssetCache* cache)
{
    BEE_ASSERT_NO_INFLIGHT_OPERATIONS();

    const auto index = find_cache_no_lock(cache);
    if (BEE_FAIL_F(index >= 0, "Cache is not registered"))
    {
        return;
    }

    for (auto& type : cache_infos_[index].supported_types)
    {
        cache_map_.erase(type);
    }

    cache_infos_.erase(index);
}

bool AssetSystem::locate_asset_no_lock(const GUID& guid, io::FileStream* stream)
{
    BEE_ASSERT(stream != nullptr);

    for (auto registry : registry_infos_)
    {
        if (registry->locate_asset(guid, stream))
        {
            return true;
        }
    }

    return false;
}

AssetData* AssetSystem::find_cached_asset(const GUID& guid)
{
    scoped_spinlock_t lock(asset_map_mutex_);
    auto asset = asset_map_.find(guid);
    if (asset == nullptr)
    {
        return nullptr;
    }
    return &asset->value;
}

i32 AssetSystem::find_loader_for_type_no_lock(const Type& type)
{
    for (auto loader : enumerate(loader_infos_))
    {
        if (container_index_of(loader.value.supported_types, [&](const Type& t) { return t == type; }) >= 0)
        {
            return loader.index;
        }
    }

    return -1;
}


i32 AssetSystem::find_cache_for_type_no_lock(const Type& type)
{
    auto index = cache_map_.find(type);
    if (index != nullptr)
    {
        return index->value;
    }
    return 0; // zero is the default cache
}


const char* get_guid_string_thread_safe(const GUID& guid)
{
    static thread_local char buffer[33];
    guid_to_string(guid, GUIDFormat::digits, buffer, static_array_length(buffer));
    return buffer;
}


LoadAssetJob::LoadAssetJob(
    const AssetLoadRequest* load_requests,
    AssetData* dst_assets,
    const i32 request_count,
    bool sync
) : assets(dst_assets)
{
    requests = FixedArray<AssetLoadRequest>::with_size(request_count, sync ? temp_allocator()  : job_temp_allocator());
    memcpy(requests.data(), load_requests, sizeof(AssetLoadRequest) * request_count);
}

void LoadAssetJob::execute()
{
    for (auto req : enumerate(requests))
    {
        auto& asset = assets[req.index];

        // We can skip the asset if it was already found in the cache and is valid
        if (asset.is_valid() && req.value.mode == AssetLoadMode::load)
        {
            continue;
        }

        // Skip any assets whose loader_info couldn't be found before kicking off the job
        if (asset.loader() < 0)
        {
            continue;
        }

        // Load the compiled asset data off disk
        io::FileStream stream;
        if (!g_asset_system.locate_asset_no_lock(req.value.asset_guid, &stream))
        {
            log_error("Unable to locate asset: %s", get_guid_string_thread_safe(req.value.asset_guid));
            continue;
        }

        auto handle = asset.handle();

        auto& loader_info = g_asset_system.loader_infos_[asset.loader()];
        auto& cache_info = g_asset_system.cache_infos_[asset.cache()];

        // Load or reload the asset - write lock ensures we don't race with any reads from the loader
        if (!handle.is_valid())
        {
            scoped_rw_write_lock_t cache_allocate_lock(cache_info.mutex);
            handle = cache_info.cache->allocate(asset.type());
            if (!handle.is_valid())
            {
                log_error("Failed to allocate asset: %s", get_guid_string_thread_safe(req.value.asset_guid));
                continue;
            }
        }

        AssetPtr ptr(cache_info.cache->get(asset.type(), handle), asset.type());
        if (!loader_info.loader->load_asset(req.value.mode, ptr, &stream))
        {
            log_error("Failed to load asset (type: %s): %s", asset.type().name, get_guid_string_thread_safe(req.value.asset_guid));
            continue;
        }

        new (&asset) AssetData(asset.type(), handle, asset.cache(), asset.loader());

        // Cache the asset as loaded if we're not reloading (i.e. already cached)
        if (req.value.mode == AssetLoadMode::load)
        {
            scoped_spinlock_t lock(g_asset_system.asset_map_mutex_);
            g_asset_system.asset_map_.insert(req.value.asset_guid, asset);
        }
    }

    --g_asset_system.loads_in_flight_;
}

void AssetSystem::load_assets(JobGroup* group, const AssetLoadRequest* load_requests, AssetData* dst_assets, const i32 count, bool sync)
{
    ++loads_in_flight_;

    Type asset_type{};

    for (int req = 0; req < count; ++req)
    {
        // Unload and invalidate the asset if we're not reloading and the asset is valid
        if (dst_assets[req].is_valid() && load_requests[req].mode == AssetLoadMode::reload)
        {
            continue;
        }

        dst_assets[req].invalidate();

        const auto cached = find_cached_asset(load_requests[req].asset_guid);
        if (cached != nullptr)
        {
            new (dst_assets + req) AssetData(*cached);
            continue;
        }

        asset_type = dst_assets[req].type();
        const auto loader_idx = find_loader_for_type_no_lock(asset_type);

        if (loader_idx < 0)
        {
            log_error("Unable to find a loader for asset (type: %s): %s", asset_type.name, get_guid_string_thread_safe(load_requests[req].asset_guid));
            continue;
        }

        const auto cache_index = find_cache_for_type_no_lock(asset_type);
        new (dst_assets + req) AssetData(asset_type, AssetHandle(), cache_index, loader_idx);
    }

    if (sync)
    {
        LoadAssetJob job(load_requests, dst_assets, count, sync);
        job.execute();
    }
    else
    {
        auto job = allocate_job<LoadAssetJob>(load_requests, dst_assets, count, sync);
        job_schedule(group, job);
    }
}

void AssetSystem::unload_asset(AssetData* asset, const AssetUnloadMode mode)
{
    ++unloads_in_flight_;

    if (!asset->is_valid())
    {
        log_error("Failed to unload asset: invalid asset data");
        return;
    }

    auto& loader_info = loader_infos_[asset->loader()];
    auto& cache_info = cache_infos_[asset->cache()];

    scoped_rw_write_lock_t unload_lock(cache_info.mutex);
    AssetPtr ptr(cache_info.cache->get(asset->type(), asset->handle()), asset->type());
    loader_info.loader->unload_asset(mode, ptr);
    cache_info.cache->deallocate(asset->type(), asset->handle());

    --unloads_in_flight_;
}

} // namespace bee