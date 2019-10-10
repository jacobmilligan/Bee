/*
 *  Asset.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Asset/Asset.hpp"
#include "Asset.hpp"


namespace bee {


AssetData::AssetData(AssetSystem* owner, const Type& type, const i32 loader, const AssetHandle& handle)
    : owner_(owner),
      type_(type),
      loader_(loader),
      handle_(handle)
{}

AssetData::~AssetData()
{
    invalidate();
}

void AssetData::unload(const AssetUnloadMode mode)
{
    BEE_ASSERT(owner_ != nullptr);
    BEE_ASSERT(loader_ >= 0);
    BEE_ASSERT(handle_.is_valid());
    owner_->unload_asset(this, mode);
    handle_ = AssetHandle();
}

void AssetData::invalidate()
{
    if (is_valid())
    {
        unload();
    }

    owner_ = nullptr;
    loader_ = -1;
    handle_ = AssetHandle();
}


void AssetSystem::register_loader(const Type& type, AssetLoader* loader)
{
    scoped_rw_write_lock_t lock(loader_mutex_);

    const auto index = find_loader_no_lock(type);
    if (BEE_FAIL_F(index < 0, "Loader \"%s\" is already registered", type.name))
    {
        return;
    }

    loader_infos_.emplace_back(type, loader);
}

void AssetSystem::unregister_loader(const Type& type)
{
    scoped_rw_write_lock_t lock(loader_mutex_);

    const auto index = find_loader_no_lock(type);
    if (BEE_FAIL_F(index >= 0, "Loader \"%s\" is not registered", type.name))
    {
        return;
    }

    loader_infos_.erase(index);
}

void AssetSystem::add_registry(const Type& type, AssetRegistry* registry)
{
    scoped_spinlock_t lock(registry_mutex_);

    const auto index = find_registry_no_lock(type);
    if (BEE_FAIL_F(index < 0, "Registry \"%s\" is already added", type.name))
    {
        return;
    }

    registry_infos_.emplace_back(type, registry);
}

void AssetSystem::remove_registry(const Type& type)
{
    scoped_spinlock_t lock(registry_mutex_);

    const auto index = find_registry_no_lock(type);
    if (BEE_FAIL_F(index >= 0, "Registry \"%s\" is not registered", type.name))
    {
        return;
    }

    registry_infos_.erase(index);
}

bool AssetSystem::locate_asset(const GUID& guid, io::FileStream* stream)
{
    BEE_ASSERT(stream != nullptr);

    scoped_spinlock_t lock(registry_mutex_);

    for (auto info : registry_infos_)
    {
        if (info.registry->locate_asset(guid, stream))
        {
            return true;
        }
    }

    return false;
}

AssetData* AssetSystem::find_cached_asset(const GUID& guid)
{
    scoped_spinlock_t lock(asset_cache_mutex_);
    auto asset = asset_cache_.find(guid);
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
        if (container_index_of(loader.value.supported_types, [&](const Type& t) { return t == type; }))
        {
            return loader.index;
        }
    }

    return -1;
}


const char* get_guid_string_thread_safe(const GUID& guid)
{
    static thread_local char buffer[33];
    guid_to_string(guid, GUIDFormat::digits, buffer, static_array_length(buffer));
    return buffer;
}


AssetSystem::LoadAssetJob::LoadAssetJob(
    AssetSystem* system,
    const AssetLoadRequest* load_requests,
    AssetData* dst_assets,
    const i32 request_count
) : asset_system(system),
    assets(dst_assets)
{
    requests = FixedArray<AssetLoadRequest>::with_size(request_count, job_temp_allocator());
    memcpy(requests.data(), load_requests, sizeof(AssetLoadRequest) * request_count);
}

void AssetSystem::LoadAssetJob::execute()
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
        if (!asset_system->locate_asset(req.value.asset_guid, &stream))
        {
            log_error("Unable to locate asset: %s", get_guid_string_thread_safe(req.value.asset_guid));
            continue;
        }

        auto handle = asset.handle();

        scoped_rw_read_lock_t loader_list_lock(asset_system->loader_mutex_);
        auto& loader_info = asset_system->loader_infos_[asset.loader()];

        // Load or reload the asset
        if (!handle.is_valid())
        {
            scoped_rw_write_lock_t loader_create_lock(loader_info.mutex);
            handle = loader_info.loader->allocate_asset(asset.type(), system_allocator());
            if (!handle.is_valid())
            {
                log_error("Failed to allocate asset: %s", get_guid_string_thread_safe(req.value.asset_guid));
                continue;
            }
        }

        if (!loader_info.loader->load_asset(handle, req.value.mode, asset.type(), &stream, system_allocator()))
        {
            log_error("Failed to load asset (type: %s): %s", asset.type().name, get_guid_string_thread_safe(req.value.asset_guid));
            continue;
        }

        new (&asset) AssetData(asset_system, asset.type(), asset.loader(), handle);

        // Cache the asset as loaded if we're not reloading (i.e. already cached)
        if (req.value.mode == AssetLoadMode::load)
        {
            scoped_spinlock_t lock(asset_system->asset_cache_mutex_);
            asset_system->asset_cache_.insert(req.value.asset_guid, asset);
        }
    }
}


void AssetSystem::load_assets(JobGroup* group, const AssetLoadRequest* load_requests, AssetData* dst_assets, const i32 count)
{
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

        scoped_rw_read_lock_t loader_lock(loader_mutex_);

        asset_type = dst_assets[req].type();
        const auto loader_idx = find_loader_for_type_no_lock(asset_type);

        if (loader_idx < 0)
        {
            log_error("Unable to find a loader for asset (type: %s): %s", asset_type.name, get_guid_string_thread_safe(load_requests[req].asset_guid));
            continue;
        }

        new (dst_assets + req) AssetData(this, asset_type, loader_idx, AssetHandle());
    }

    auto job = allocate_job<LoadAssetJob>(this, load_requests, dst_assets, count);
    job_schedule(group, job);
}

void AssetSystem::unload_asset(AssetData* asset, const AssetUnloadMode mode)
{
    BEE_ASSERT(asset->owner() == this);
    scoped_rw_read_lock_t loader_list_lock(loader_mutex_);

    if (!asset->is_valid())
    {
        log_error("Failed to unload asset: invalid asset data");
        return;
    }

    auto& loader_info = loader_infos_[asset->loader()];

    scoped_rw_write_lock_t unload_lock(loader_info.mutex);
    loader_info.loader->unload_asset(asset->handle(), mode, asset->type());
}


} // namespace bee