/*
 *  AssetRegistry.cpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#include "Bee.AssetRegistry.Descriptor.hpp"
#include "Bee/Plugins/AssetRegistry/AssetRegistry.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/Containers/ResourcePool.hpp"
#include "Bee/Core/Plugin.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"
#include "Bee/Core/Jobs/JobDependencyCache.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"


namespace bee {


struct AssetCache
{
    RecursiveSpinLock                   mutex;
    ResourcePool<AssetId, AssetData>    cache;
    DynamicHashMap<GUID, AssetId>       guid_to_id;
    DynamicHashMap<u32, GUID>           name_to_guid;

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

    AssetData* find(const StringView& name)
    {
        scoped_recursive_spinlock_t lock(mutex);

        auto* guid = name_to_guid.find(get_hash(name));

        if (guid == nullptr)
        {
            return nullptr;
        }

        return find(guid->value);
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

    AssetData* insert(const StringView& name, const GUID& guid)
    {
        scoped_recursive_spinlock_t lock(mutex);

        const auto name_hash = get_hash(name);

        BEE_ASSERT(name_to_guid.find(name_hash) == nullptr);

        name_to_guid.insert(name_hash, guid);

        return insert(guid);
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
    AssetLoader*            instance { nullptr };
    TypeRef                 parameter_type { get_type<UnknownType>() };
    DynamicArray<TypeRef>   supported_types;
};

struct AssetRegistry
{
    JobDependencyCache                  job_deps;
    AssetCache                          cache;
    DynamicHashMap<u32, AssetLoader*>   type_hash_to_loader;
    DynamicArray<RegisteredLoader>      loaders;
    DynamicArray<AssetLocator*>         locators;
    DynamicArray<AssetManifest>         manifests;
};

static AssetRegistry* g_registry { nullptr };


void add_loader(AssetLoader* loader);
void remove_loader(AssetLoader* loader);
void add_locator(AssetLocator* locator);
void remove_locator(AssetLocator* locator);
bool locate_asset(const GUID& guid, AssetLocation* location);
void unload_asset_data(const AssetData* asset, const UnloadAssetMode kind);


void init_registry()
{

}

void destroy_registry()
{

}

void load_asset_job(AssetData* asset, AssetLoader* loader)
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

AssetData* get_or_load_asset_data(const GUID& guid, const TypeRef& type, const AssetLoadArg& arg)
{
    auto& cache = g_registry->cache;
    auto* cached = cache.find(guid);

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
    auto* loader = g_registry->type_hash_to_loader.find(type->hash);

    if (loader == nullptr)
    {
        log_error("Failed to find a registered loader that can handle assets of type %s", type->name);
        return nullptr;
    }

    // Now that we know we have a valid loader for the type we can add a new cache entry if needed
    if (cached == nullptr)
    {
        cached = cache.insert(guid);
        cached->type = type;
        cached->parameter_type = loader->value->get_parameter_type();
    }

    cached->loader = loader->value;

    // Don't try to reload assets currently in flight or already loaded - add a reference instead
    if (cached->status == AssetStatus::loaded || cached->status == AssetStatus::loading)
    {
        ++cached->refcount;
        return cached;
    }

    if (BEE_FAIL_F(arg.type == cached->parameter_type, "Invalid argument given to load_asset_data: expected %s but got %s", cached->parameter_type->name, arg.type->name))
    {
        return nullptr;
    }

    // Copy the load parameter to this asset
    memcpy(cached->parameter_storage, arg.data, arg.type->size);

    // Allocate new data if we're not reloading
    if (cached->ptr == nullptr)
    {
        cached->ptr = cached->loader->allocate(type);
    }

    auto* job = create_job(load_asset_job, cached, cached->loader);
    g_registry->job_deps.write(guid, job);

    return cached;
}

void unload_asset_data(AssetData* asset, const UnloadAssetMode kind)
{
    auto& cache = g_registry->cache;

    if (cache.get(asset->id) == nullptr)
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

    g_registry->job_deps.write(asset->guid, create_null_job());

    // unload and deallocate the asset if last ref or otherwise explicitly requested
    AssetLoaderContext ctx(asset);
    asset->status = asset->loader->unload(&ctx);

    if (asset->status == AssetStatus::unloaded)
    {
        cache.erase(asset->id);
    }
}

AssetManifest* add_manifest(const StringView& name)
{
    const auto hash = get_hash(name);
    auto index = find_index_if(g_registry->manifests, [&](const AssetManifest& manifest)
    {
        return manifest.id == hash;
    });

    if (index < 0)
    {
        g_registry->manifests.emplace_back();
        auto& manifest = g_registry->manifests.back();
        manifest.id = hash;
        index = g_registry->manifests.size() - 1;
    }

    return &g_registry->manifests[index];
}

void remove_manifest(const StringView& name)
{
    const auto hash = get_hash(name);
    const auto index = find_index_if(g_registry->manifests, [&](const AssetManifest& manifest)
    {
        return manifest.id == hash;
    });

    if (index >= 0)
    {
        g_registry->manifests.erase(index);
    }
}

AssetManifest* get_manifest(const StringView& name)
{
    const auto hash = get_hash(name);
    const auto index = find_index_if(g_registry->manifests, [&](const AssetManifest& manifest)
    {
        return manifest.id == hash;
    });

    return index >= 0 ? &g_registry->manifests[index] : nullptr;
}

void serialize_manifests(const SerializerMode mode, io::Stream* stream)
{
    StreamSerializer serializer(stream);
    serialize(mode, &serializer, &g_registry->manifests);
}

bool locate_asset(const GUID& guid, AssetLocation* location)
{
    for (auto& locator : g_registry->locators)
    {
        if (locator->locate(guid, location))
        {
            return true;
        }
    }

    return false;
}

void add_loader(AssetLoader* loader)
{
    auto parameter_type = loader->get_parameter_type();
    if (BEE_FAIL_F(parameter_type->size < AssetData::load_parameter_capacity, "Failed to add loader: parameter type is too large"))
    {
        return;
    }

    g_registry->job_deps.wait_all();

    const auto existing_index = find_index_if(g_registry->loaders, [&](const RegisteredLoader& l)
    {
        return l.instance == loader;
    });

    if (existing_index >= 0)
    {
        log_error("Asset loader was added multiple times to the asset registry");
        return;
    }

    g_registry->loaders.emplace_back();

    auto& registered = g_registry->loaders.back();
    registered.instance = loader;
    registered.parameter_type = parameter_type;
    registered.instance->get_supported_types(&registered.supported_types);

    // Add mappings for all the supported types to the loader API
    for (const auto& type : registered.supported_types)
    {
        auto* type_mapping = g_registry->type_hash_to_loader.find(type->hash);

        if (type_mapping != nullptr)
        {
            log_error("A loader is already registered to handle type %s", type->name);
            continue;
        }

        g_registry->type_hash_to_loader.insert(type->hash, registered.instance);
    }
}

void remove_loader(AssetLoader* loader)
{
    g_registry->job_deps.wait_all();

    const auto index = find_index_if(g_registry->loaders, [&](const RegisteredLoader& l)
    {
        return l.instance == loader;
    });

    if (index < 0)
    {
        log_error("Asset loader was not previously added to the Asset Registry");
        return;
    }

    for (const auto& type : g_registry->loaders[index].supported_types)
    {
        g_registry->type_hash_to_loader.erase(type->hash);
    }

    g_registry->loaders.erase(index);
}

void add_locator(AssetLocator* locator)
{
    g_registry->job_deps.wait_all();

    const auto existing_index = find_index_if(g_registry->locators, [&](const AssetLocator* l)
    {
        return l == locator;
    });

    if (existing_index >= 0)
    {
        log_error("Asset locator was added multiple times to the asset registry");
        return;
    }

    g_registry->locators.emplace_back(locator);
}

void remove_locator(AssetLocator* locator)
{
    g_registry->job_deps.wait_all();

    const auto index = find_index_if(g_registry->locators, [&](const AssetLocator* l)
    {
        return l == locator;
    });

    if (index < 0)
    {
        log_error("Asset locator was not previously added to the Asset Registry");
        return;
    }

    g_registry->locators.erase(index);
}

} // namespace bee


static bee::AssetRegistryModule g_module{};


BEE_PLUGIN_API void bee_load_plugin(bee::PluginRegistry* registry, const bee::PluginState state)
{
    bee::g_registry = registry->get_or_create_persistent<bee::AssetRegistry>("BeeAssetRegistry");

    g_module.init = bee::init_registry;
    g_module.destroy = bee::destroy_registry;
    g_module.load_asset_data = bee::get_or_load_asset_data;
    g_module.unload_asset_data = bee::unload_asset_data;
    g_module.add_manifest = bee::add_manifest;
    g_module.remove_manifest = bee::remove_manifest;
    g_module.get_manifest = bee::get_manifest;
    g_module.serialize_manifests = bee::serialize_manifests;
    g_module.add_loader = bee::add_loader;
    g_module.remove_loader = bee::remove_loader;
    g_module.add_locator = bee::add_locator;
    g_module.remove_locator = bee::remove_locator;

    registry->toggle_module(state, BEE_ASSET_REGISTRY_MODULE_NAME, &g_module);
}