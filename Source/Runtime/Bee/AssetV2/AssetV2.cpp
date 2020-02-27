/*
 *  AssetV2.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetV2/AssetV2.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Thread.hpp"
#include "Bee/Core/Containers/ResourcePool.hpp"
#include "Bee/Core/Atomic.hpp"

namespace bee {


struct AssetLoaderInfo
{
    String                      name;
    u32                         name_hash { 0 };
    DynamicArray<const Type*>   supported_types;
};

struct AssetLocatorInfo
{
    String                  name;
    u32                     name_hash { 0 };
};


struct AssetCacheEntry
{
    AssetInfo   info;
    void*       asset_ptr { nullptr };
};


struct AssetCache
{
    ReaderWriterMutex                           mutex;
    ResourcePool<AssetHandle, AssetCacheEntry>  data;
    DynamicHashMap<GUID, AssetHandle>           lookup;

    AssetCache() noexcept
        : data(sizeof(AssetInfo) * 64)
    {}

    AssetCacheEntry* find(const GUID& guid)
    {
        scoped_rw_read_lock_t lock(mutex);

        auto existing_data = lookup.find(guid);
        if (existing_data != nullptr)
        {
            return &data[existing_data->value];
        }
        return nullptr;
    }

    AssetCacheEntry* insert(void* asset_ptr, const GUID& guid, const Type* type, const i32 loader)
    {
        scoped_rw_write_lock_t lock(mutex);

        BEE_ASSERT(lookup.find(guid) == nullptr);
        const auto handle = data.allocate();
        auto cached = &data[handle];
        cached->info.handle = handle;
        cached->info.status = AssetStatus::unloaded;
        cached->info.type = type;
        cached->info.guid = guid;
        cached->info.loader = loader;
        cached->asset_ptr = asset_ptr;
        lookup.insert(guid, handle);
        return cached;
    }

    void erase(const GUID& guid)
    {
        scoped_rw_write_lock_t lock(mutex);

        auto existing_data = lookup.find(guid);
        if (existing_data == nullptr)
        {
            return;
        }

        data.deallocate(existing_data->value);
        lookup.erase(guid);
    }
};


static DynamicArray<AssetLoader*>           g_loaders;
static DynamicArray<AssetLoaderInfo>        g_loader_infos;
static DynamicArray<AssetLocator*>          g_locators;
static DynamicArray<AssetLocatorInfo>       g_locator_infos;
static DynamicHashMap<String, GUID>         g_name_map;
static AssetCache                           g_cache;
static JobGroup                             g_jobs_in_progress;


i32 find_loader(const u32 hash)
{
    return container_index_of(g_loader_infos, [&](const AssetLoaderInfo& info) { return info.name_hash == hash; });
}

i32 find_locator(const u32 hash)
{
    return container_index_of(g_locator_infos, [&](const AssetLocatorInfo& info) { return info.name_hash == hash; });
}

void complete_jobs_main_thread()
{
    BEE_ASSERT(current_thread::is_main());
    job_wait(&g_jobs_in_progress);
}


void assets_init()
{

}

void assets_shutdown()
{
    complete_jobs_main_thread();

    // TODO(Jacob): unload all loaded assets

    g_loaders.clear();
    g_locators.clear();
    g_cache.data.clear();
    g_cache.lookup.clear();
}

void register_asset_name(const char* name, const GUID& guid)
{
    BEE_ASSERT(current_thread::is_main());

    if (BEE_FAIL_F(g_name_map.find(name) == nullptr, "Asset is already registered with the name \"%s\"", name))
    {
        return;
    }

    g_name_map.insert(String(name), guid);
}

void unregister_asset_name(const char* name)
{
    BEE_ASSERT(current_thread::is_main());

    if (BEE_FAIL_F(g_name_map.find(name) != nullptr, "No asset is registered with the name \"%s\"", name))
    {
        return;
    }

    g_name_map.erase(name);
}

void register_asset_loader(const char* name, AssetLoader* loader, std::initializer_list<const Type*> supported_types)
{
    complete_jobs_main_thread();

    const auto name_hash = get_hash(name);
    if (BEE_FAIL_F(find_loader(name_hash) < 0, "failed to register asset loader: a loader with the name \"%s\" is already registered", name))
    {
        return;
    }

    AssetLoaderInfo info;
    info.name = name;
    info.name_hash = name_hash;
    g_loaders.push_back(loader);
    g_loader_infos.push_back(info);
    g_loader_infos.back().supported_types.append({ supported_types.begin(), static_cast<i32>(supported_types.size()) });
}

void unregister_asset_loader(const char* name)
{
    complete_jobs_main_thread();

    const auto loader_idx = find_loader(get_hash(name));
    if (BEE_FAIL_F(loader_idx >= 0, "failed to unregister asset loader: no loader is registered with the name \"%s\"", name))
    {
        return;
    }

    g_loaders.erase(loader_idx);
    g_loader_infos.erase(loader_idx);
}

void register_asset_locator(const char* name, AssetLocator* locator)
{
    complete_jobs_main_thread();

    const auto name_hash = get_hash(name);
    if (BEE_FAIL_F(find_locator(name_hash) < 0, "failed to register asset locator: a locator with the name \"%s\" is already registered", name))
    {
        return;
    }

    AssetLocatorInfo info;
    info.name = name;
    info.name_hash = name_hash;
    g_locators.push_back(locator);
    g_locator_infos.push_back(info);
}

void unregister_asset_locator(const char* name)
{
    complete_jobs_main_thread();

    const auto locator_idx = find_locator(get_hash(name));
    if (BEE_FAIL_F(locator_idx >= 0, "failed to unregister asset loader: no loader is registered with the name \"%s\"", name))
    {
        return;
    }

    g_locators.erase(locator_idx);
}

bool asset_name_to_guid(const char* name, GUID* dst_guid)
{
    BEE_ASSERT(current_thread::is_main());

    auto name_mapping = g_name_map.find(name);
    if (name_mapping == nullptr)
    {
        return false;
    }

    *dst_guid = name_mapping->value;
    return true;
}


void load_asset_job(void* asset_ptr, AssetInfo* info, AssetLoader* loader)
{
    info->status = AssetStatus::loading;

    AssetLocation location;

    for (AssetLocator* locator : g_locators)
    {
        if (locator->locate(info->guid, &location))
        {
            break;
        }
    }

    if (location.type == AssetLocationType::invalid)
    {
        log_error("Failed to locate asset %s", guid_to_string(info->guid, GUIDFormat::digits, temp_allocator()).c_str());
        return;
    }

    AssetData data(info->type, asset_ptr);

    switch (location.type)
    {
        case AssetLocationType::in_memory:
        {
            io::MemoryStream stream(location.read_only_buffer, location.read_only_buffer_size);
            info->status = loader->load(&data, &stream);
            break;
        }
        case AssetLocationType::file:
        {
            io::FileStream stream(location.file_path, "rb");
            info->status = loader->load(&data, &stream);
            break;
        }
        default:
        {
            BEE_UNREACHABLE("Invalid asset location type");
        }
    }
}


bool request_asset_load(const GUID& guid, const Type* requested_type, AssetInfo** info, AssetData* data)
{
    auto cached = g_cache.find(guid);
    if (cached != nullptr)
    {
        if (cached->info.status == AssetStatus::loaded || cached->info.status == AssetStatus::loading)
        {
            *info = &cached->info;
            *data = AssetData(cached->info.type, cached->asset_ptr);
            return true;
        }
    }

    // Find the loader for the requested type
    const auto loader_idx = container_index_of(g_loader_infos, [&](const AssetLoaderInfo& info)
    {
        return container_index_of(info.supported_types, [&](const Type* type) { return type == requested_type; }) >= 0;
    });

    if (BEE_FAIL_F(loader_idx >= 0, "Failed to load asset: no loaders are registered for type %s", requested_type->name))
    {
        return false;
    }

    // add to cache if missing
    auto loader = g_loaders[loader_idx];
    if (cached == nullptr)
    {
        auto new_asset_ptr = loader->allocate(requested_type);
        cached = g_cache.insert(new_asset_ptr, guid, requested_type, loader_idx);
    }

    BEE_ASSERT(cached->info.handle.is_valid());

    *info = &cached->info;
    *data = AssetData(cached->info.type, cached->asset_ptr);

    // Kick the load job
    auto job = create_job(load_asset_job, cached->asset_ptr, &cached->info, loader);
    job_schedule(&g_jobs_in_progress, job);

    return true;
}

void unload_asset(AssetInfo* info, const AssetUnloadType unload_type)
{
    BEE_ASSERT(info->loader >= 0);

    AssetData data(info->type, g_cache.data[info->handle].asset_ptr);
    info->status = g_loaders[info->loader]->unload(&data, unload_type);
    if (info->status == AssetStatus::unloaded)
    {
        g_cache.erase(info->guid);
    }
}


} // namespace bee