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

#include <atomic>

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

struct AssetCache
{
    ResourcePool<AssetHandle, AssetInfo>    data { sizeof(AssetInfo) * 64 };
    DynamicHashMap<GUID, AssetHandle>       lookup;

    AssetCache()
        : data(sizeof(AssetInfo) * 64)
    {}

    AssetInfo* find(const GUID& guid)
    {
        auto existing_data = lookup.find(guid);
        if (existing_data != nullptr)
        {
            return &data[existing_data->value];
        }
        return nullptr;
    }

    AssetInfo* insert(const GUID& guid, const Type* type, const i32 loader)
    {
        BEE_ASSERT(lookup.find(guid) == nullptr);
        const auto handle = data.allocate();
        auto cached = &data[handle];
        cached->status = AssetStatus::unloaded;
        cached->type = type;
        cached->guid = guid;
        cached->loader = loader;
        lookup.insert(guid, handle);
        return cached;
    }

    void erase(const GUID& guid)
    {
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
static std::mutex                           g_mutex;


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
    std::lock_guard<std::mutex> lock(g_mutex);

    job_wait(&g_jobs_in_progress);
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

struct RequestAssetJob final : public Job
{
    AssetInfo*      info { nullptr };
    AssetLoader*    loader { nullptr };

    RequestAssetJob(AssetInfo* req_info, AssetLoader* req_loader)
        : info(req_info),
          loader(req_loader)
    {}

    void execute() override
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

        auto data = loader->get(info->type, info->handle);

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
};

bool request_asset_load(const GUID& guid, const Type* requested_type, AssetInfo** info, AssetData* data)
{
    if (!current_thread::is_main())
    {
        log_error("Asset requests must be kicked from the main thread");
        return false;
    }

    auto cached = g_cache.find(guid);
    if (cached != nullptr)
    {
        if (cached->status == AssetStatus::loaded || cached->status == AssetStatus::loading)
        {
            *info = cached;
            *data = g_loaders[cached->loader]->get(cached->type, cached->handle);
            return true;
        }
    }

    // Find the loader for the requested type
    const auto loader_idx = container_index_of(g_loader_infos, [&](const AssetLoaderInfo& info)
    {
        return container_index_of(info.supported_types, [&](const Type* type) { return type == requested_type; }) >= 0;
    });

    if (BEE_FAIL_F(loader_idx >= 0, "Failed to load asset: no loaders are registered type %s", requested_type->name))
    {
        return false;
    }

    if (cached == nullptr)
    {
        cached = g_cache.insert(guid, requested_type, loader_idx);
    }

    auto loader = g_loaders[loader_idx];
    if (!cached->handle.is_valid())
    {
        cached->handle = loader->allocate(requested_type);
    }

    BEE_ASSERT(cached->handle.is_valid());

    auto job = allocate_job<RequestAssetJob>(cached, loader);
    job_schedule(&g_jobs_in_progress, job);

    *info = cached;
    *data = loader->get(requested_type, cached->handle);
    return true;
}

void unload_asset(AssetInfo* info, const AssetUnloadType unload_type)
{
    BEE_ASSERT(info->loader >= 0);

    if (!current_thread::is_main())
    {
        log_error("Asset unloads must be called from the main thread");
        return;
    }

    auto data = g_loaders[info->loader]->get(info->type, info->handle);
    info->status = g_loaders[info->loader]->unload(&data, unload_type);
    if (info->status == AssetStatus::unloaded)
    {
        g_cache.erase(info->guid);
    }
}


} // namespace bee