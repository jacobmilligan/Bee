/*
 *  AssetLoadPipeline.cpp
 *  Bee
 *
 *  Copyright (c) 2021 Jacob Milligan. All rights reserved.
 */

#include "Bee/AssetPipeline/AssetPipeline.inl"

#include "Bee/Core/Plugin.hpp"


namespace bee {


extern AssetDatabaseModule g_assetdb;

Result<void, AssetPipelineError> init_load_pipeline(AssetPipeline* pipeline)
{
    return {};
}

void destroy_load_pipeline(AssetPipeline* pipeline)
{

}

static LoaderId find_loader(LoadPipeline* pipeline, const AssetLoader* loader)
{
    for (const auto& l : pipeline->loaders)
    {
        if (l.resource.instance == loader)
        {
            return l.handle;
        }
    }
    return LoaderId{};
}

Result<void, AssetPipelineError> register_loader(AssetPipeline* pipeline, AssetLoader* loader, void* user_data)
{
    if (!pipeline->can_load())
    {
        return { AssetPipelineError::load };
    }

    auto& load_pipeline = pipeline->load;

    // Get the supported type list for this loader
    FixedArray<Type> types;
    const int type_count = loader->get_types(nullptr);
    BEE_ASSERT(type_count > 0);
    types.resize(type_count);

    loader->get_types(types.data());

    // Validate the supported types are not already registered to a different loader
    for (const auto& type : types)
    {
        if (load_pipeline.type_to_loader.find(type) != nullptr)
        {
            return { AssetPipelineError::loader_type_conflict };
        }
    }

    // Register the loader and type mappings
    const auto id = load_pipeline.loaders.allocate();
    auto& registered_loader = load_pipeline.loaders[id];
    registered_loader.types = BEE_MOVE(types);
    registered_loader.instance = loader;
    registered_loader.user_data = user_data;

    for (const auto& type : registered_loader.types)
    {
        load_pipeline.type_to_loader.insert(type, id);
    }

    return {};
}

Result<void, AssetPipelineError> unregister_loader(AssetPipeline* pipeline, AssetLoader* loader)
{
    if (!pipeline->can_load())
    {
        return { AssetPipelineError::load };
    }

    auto& load_pipeline = pipeline->load;
    const auto id = find_loader(&load_pipeline, loader);

    if (!id.is_valid())
    {
        return { AssetPipelineError::invalid_loader };
    }

    auto& registered_loader = load_pipeline.loaders[id];

    // Do one final tick in case the loaders tick requires destroying cached resources etc.
    if (registered_loader.instance->tick != nullptr)
    {
        registered_loader.instance->tick(registered_loader.user_data);
    }

    for (const auto& type : registered_loader.types)
    {
        load_pipeline.type_to_loader.erase(type);
    }

    load_pipeline.loaders.deallocate(id);
    return {};
}

Result<void, AssetPipelineError> register_locator(AssetPipeline* pipeline, AssetLocator* locator)
{
    if (!pipeline->can_load())
    {
        return { AssetPipelineError::load };
    }

    const int index = find_index(pipeline->load.locators, locator);
    if (index < 0)
    {
        pipeline->load.locators.push_back(locator);
    }

    return {};
}

Result<void, AssetPipelineError> unregister_locator(AssetPipeline* pipeline, AssetLocator* locator)
{
    if (!pipeline->can_load())
    {
        return { AssetPipelineError::load };
    }

    const int index = find_index(pipeline->load.locators, locator);
    if (index >= 0)
    {
        pipeline->load.locators.erase(index);
    }
    return {};
}

static void add_cached_asset(LoadPipeline* pipeline, const AssetKey& key, const AssetHandle handle)
{
    scoped_recursive_lock_t lock(pipeline->cache_mutex);
    pipeline->cache.insert(get_hash(key), handle);
}

static AssetHandle find_cached_asset(LoadPipeline* pipeline, const AssetKey& key)
{
    scoped_recursive_lock_t lock(pipeline->cache_mutex);
    auto* handle = pipeline->cache.find(get_hash(key));
    return handle == nullptr ? AssetHandle{} : handle->value;
}

//static void remove_cached_asset(LoadPipeline* pipeline, const GUID guid)
//{
//    scoped_recursive_lock_t lock(pipeline->mutex);
//    pipeline->cache.erase(guid);
//}

static bool locate_asset_database_asset(AssetPipeline* pipeline, const AssetKey& key, const Type type, AssetLocation* location)
{
    auto txn = g_assetdb.read(pipeline->import.db);

    GUID guid{};
    if (key.kind == AssetKey::Kind::guid)
    {
        if (!g_assetdb.asset_exists(&txn, key.guid))
        {
            return false;
        }
        guid = key.guid;
    }
    else
    {
        auto res = g_assetdb.get_guid_from_name(&txn, key.name);
        if (!res)
        {
            return false;
        }
        guid = res.unwrap();
    }

    auto res = g_assetdb.get_artifacts(&txn, guid, nullptr);
    if (!res)
    {
        return false;
    }

    location->streams.size = res.unwrap();

    if (location->streams.size > 0)
    {
        auto* artifacts = BEE_ALLOCA_ARRAY(AssetArtifact, location->streams.size);
        g_assetdb.get_artifacts(&txn, guid, artifacts);

        location->type = get_type(artifacts[0].type_hash);

        for (int i = 0; i < location->streams.size; ++i)
        {
            location->streams[i].kind = AssetStreamInfo::Kind::file;
            location->streams[i].hash = artifacts[i].content_hash;
            location->streams[i].offset = 0;
            g_assetdb.get_artifact_path(&txn, artifacts[i].content_hash, &location->streams[i].path);
        }
    }

    return true;
}

static bool locate_asset(AssetPipeline* pipeline, const AssetKey& key, const Type type, AssetLocation* location)
{
    if (!pipeline->can_load())
    {
        return false;
    }

    for (auto& locator : pipeline->load.locators)
    {
        if (locator->locate(key, type, location, locator->user_data))
        {
            return true;
        }
    }

    // Fallback on the manifest or asset database locator otherwise
    if (pipeline->can_import())
    {
        return locate_asset_database_asset(pipeline, key, type, location);
    }

    // TODO(Jacob): manifest
    return false;
}

Result<AssetHandle, AssetPipelineError> load_asset_from_key(AssetPipeline* pipeline, const AssetKey& key, const Type type)
{
    if (!pipeline->can_load())
    {
        return { AssetPipelineError::load };
    }

    auto& load_pipeline = pipeline->load;
    auto handle = find_cached_asset(&load_pipeline, key);

    // Check if the asset has been loaded and cached previously and if so just increment the refcount and return it
    if (handle.is_valid())
    {

        const LoaderId loader_id(handle);
        const AssetId asset_id(handle);
        auto& loader = load_pipeline.loaders[loader_id];

        if (loader.assets.is_active(asset_id))
        {
            ++loader.assets[asset_id].refcount;
            return handle;
        }
    }

    // Find a matching asset with the given GUID and type
    AssetLocation location{};
    if (!locate_asset(pipeline, key, type, &location))
    {
        return { AssetPipelineError::failed_to_locate };
    }

    // Find a loader that can handle this asset type
    auto* loader_id = load_pipeline.type_to_loader.find(location.type);
    if (loader_id == nullptr)
    {
        return { AssetPipelineError::no_loader_for_type };
    }

    auto& loader = load_pipeline.loaders[loader_id->value];

    // lock the loader resource pool and allocate the asset info & id
    AssetId asset_id{};
    {
        scoped_recursive_lock_t lock(loader.mutex);
        asset_id = loader.assets.allocate();
        if (!asset_id.is_valid())
        {
            return { AssetPipelineError::failed_to_allocate };
        }
    }

    auto& asset = loader.assets[asset_id];
//    asset.guid = guid;
    asset.location = BEE_MOVE(location);
    asset.data = location.type->create_instance(system_allocator());
    asset.loader = loader_id->value;
    ++asset.refcount;

    // Load the asset
    handle = AssetHandle(loader_id->value.id, asset_id.id);
    auto res = loader.instance->load(asset.guid, &asset.location, loader.user_data, handle, asset.data.data());
    if (!res)
    {
        // Lock the pool and deallocate immediately if the load failed for whatever reason
        scoped_recursive_lock_t lock(loader.mutex);
        loader.assets.deallocate(asset_id);
        return res.unwrap_error();
    }

    // Register the new asset to be added in the global cache at the next refresh()
    add_cached_asset(&load_pipeline, key, handle);

    return handle;
}

Result<i32, AssetPipelineError> unload_asset(AssetPipeline* pipeline, const AssetHandle handle)
{
    if (!pipeline->can_load())
    {
        return { AssetPipelineError::load };
    }

    // TODO(Jacob): decrement refcount, if 0 add handle to pending unload list for the thread. Later in refresh()
    //  we'll iterate over these handles and confirm that they weren't loaded again while pending (i.e. call unload()
    //  then later call load() again for the same GUID that will increment the refcount)
    const LoaderId loader_id(handle);
    const AssetId asset_id(handle);

    auto& asset = pipeline->load.loaders[loader_id].assets[asset_id];
    int old_refcount = asset.refcount.load(std::memory_order_relaxed);
    int new_refcount = 0;

    // Try and decrement the refcount - if we win the race against another thread incrementing the refcount then
    // we can add it to the list of deferred asset handles to unload later on.
    do
    {
        // This will be true if either the asset had its refcount decremented before the function started or if
        // we lost a race against another thread to decrement it but the result was zero - in which case the asset has
        // already been added to the deferred unload list by a different thread
        if (old_refcount == 0)
        {
            return old_refcount;
        }
        new_refcount = old_refcount - 1;
    } while (!asset.refcount.compare_exchange_weak(old_refcount, new_refcount, std::memory_order_seq_cst));

    // We'll only enter this code path here if we won the race - so now add to the list of pending unloads to check
    // next frame
    auto& thread = pipeline->get_thread();
    if (find_index(thread.pending_unloads, handle) < 0)
    {
        thread.pending_unloads.push_back(handle);
    }

    return 0;
}

Result<void, AssetPipelineError> refresh_load_pipeline(AssetPipeline* pipeline)
{
    auto& load_pipeline = pipeline->load;

    for (auto loader : pipeline->load.loaders)
    {
        if (loader.resource.instance->tick != nullptr)
        {
            loader.resource.instance->tick(loader.resource.user_data);
        }
    }

    for (auto& thread : pipeline->thread_data)
    {
        for (auto& handle : thread.pending_unloads)
        {
            const LoaderId loader_id(handle);
            const AssetId asset_id(handle);

            auto& loader = load_pipeline.loaders[loader_id];
            if (!loader.assets.is_active(asset_id))
            {
                // the asset was already unloaded by another thread
                continue;
            }

            auto& asset = loader.assets[asset_id];
            if (asset.refcount.load(std::memory_order_relaxed) > 0)
            {
                // the asset refcount was incremented via a reload between the time we called unload() and now
                continue;
            }

            // Unload the asset
            auto res = loader.instance->unload(asset.location.type, asset.data.data(), loader.user_data);
            if (!res)
            {
                return res.unwrap_error();
            }

            // Remove the cached and stored data from the loader and global cache respectively
            load_pipeline.cache.erase(get_hash(asset.guid));
            loader.assets.deallocate(asset_id);
        }

        thread.pending_unloads.clear();
    }

    return {};
}

Result<void*, AssetPipelineError> get_asset_data(AssetPipeline* pipeline, const AssetHandle handle)
{
    if (!pipeline->can_load())
    {
        return { AssetPipelineError::load };
    }

    const LoaderId loader_id(handle);
    const AssetId asset_id(handle);

    if (!loader_id.is_valid() || !asset_id.is_valid())
    {
        return { AssetPipelineError::invalid_asset_handle };
    }

    auto& asset = pipeline->load.loaders[loader_id].assets[asset_id];
    return asset.data.data();
}

bool is_asset_loaded(AssetPipeline* pipeline, const AssetKey& key)
{
    if (!pipeline->can_load())
    {
        return false;
    }

    return find_cached_asset(&pipeline->load, key).is_valid();
}

void set_load_pipeline(AssetPipelineModule* module, PluginLoader* loader, const PluginState state)
{
    module->register_loader = register_loader;
    module->unregister_loader = unregister_loader;
    module->register_locator = register_locator;
    module->unregister_locator = unregister_locator;
    module->load_asset_from_key = load_asset_from_key;
    module->unload_asset = unload_asset;
    module->get_asset_data = get_asset_data;
    module->is_asset_loaded = is_asset_loaded;
    module->locate_asset = locate_asset;
}


} // namespace bee
