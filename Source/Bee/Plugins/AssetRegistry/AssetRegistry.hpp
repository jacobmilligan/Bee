/*
 *  AssetRegistry.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Handle.hpp"
#include "Bee/Core/GUID.hpp"
#include "Bee/Core/Atomic.hpp"

namespace bee {


enum class AssetStatus
{
    invalid,
    unloaded,
    loading,
    loading_failed,
    loaded
};

enum class UnloadAssetMode
{
    release,
    destroy
};

enum class AssetStreamType
{
    none,
    file,
    buffer
};

class AssetLoaderContext;
struct AssetLocation;
struct AssetRegistryModule;
struct AssetLoader;
template <typename T>
class Asset;


BEE_VERSIONED_HANDLE_64(AssetId);

struct BEE_REFLECT(serializable, version = 1) AssetManifest
{
    u32                 id { 0 };
    FixedArray<u32>     asset_hashes;
    FixedArray<GUID>    asset_guids;

    explicit AssetManifest(Allocator* allocator = system_allocator())
        : asset_hashes(allocator),
          asset_guids(allocator)
    {}

    explicit AssetManifest(const i32 asset_count, const u32* hashes, const GUID* guids, Allocator* allocator = system_allocator())
    {
        asset_hashes = FixedArray<u32>::with_size(asset_count, allocator);
        asset_guids = FixedArray<GUID>::with_size(asset_count, allocator);

        memcpy(asset_hashes.data(), hashes, sizeof(u32) * asset_count);
        memcpy(asset_guids.data(), guids, sizeof(GUID) * asset_count);
    }

    inline GUID get(const u32 hash) const
    {
        const auto index = find_index(asset_hashes, hash);
        return index >= 0 ? asset_guids[index] : GUID{};
    }

    template <i32 Size>
    inline GUID get(const char(&name)[Size]) const
    {
        return get(get_static_string_hash(name));
    }

    inline bool add(const u32 hash, const GUID& guid)
    {
        const auto index = find_index(asset_hashes, hash);
        if (index >= 0)
        {
            return false;
        }

        asset_hashes.resize(asset_hashes.size() + 1);
        asset_guids.resize(asset_guids.size() + 1);
        asset_hashes.back() = hash;
        asset_guids.back() = guid;
        return true;
    }

    template <i32 Size>
    inline bool add(const char(&name)[Size], const GUID& guid)
    {
        return add(get_static_string_hash(name), guid);
    }

    template <typename T, i32 Size>
    inline Asset<T> load(AssetRegistryModule* registry, const char(&name)[Size]);

    template <typename T, typename ArgType, i32 Size>
    inline Asset<T> load(AssetRegistryModule* registry, const char(&name)[Size], const ArgType& arg);
};

struct AssetLoadArg
{
    TypeRef type { get_type<UnknownType>() };
    const void* data { nullptr };
};

struct AssetData
{
    static constexpr i32 load_arg_capacity = 128;

    GUID                    guid;
    AssetId                 id;
    AssetStatus             status { AssetStatus::invalid };
    AssetLoader*            loader { nullptr };
    std::atomic_int32_t     refcount { 0 };
    TypeRef                 type { nullptr };
    void*                   ptr { nullptr };
    TypeRef                 parameter_type { nullptr };
    u8                      argument_storage[load_arg_capacity];
};

struct AssetStreamInfo
{
    TypeRef             asset_type;
    AssetStreamType     stream_type { AssetStreamType::none };
    size_t              offset { 0 };
    Path                path;
    void*               buffer { nullptr };
    size_t              buffer_size { 0 };
};

struct AssetLocation
{
    static constexpr i32 max_streams = 8;

    TypeRef         type { nullptr };
    i32             stream_count { 0 };
    AssetStreamInfo streams[max_streams];

    inline void clear()
    {
        stream_count = 0;
        type = TypeRef{};
        for (auto& stream : streams)
        {
            stream.asset_type = TypeRef{};
            stream.stream_type = AssetStreamType::none;
            stream.offset = 0;
            stream.path.clear();
            stream.buffer = nullptr;
            stream.buffer_size = 0;
        }
    }
};

struct AssetLoader
{
    i32 (*get_supported_types)(TypeRef* types) { nullptr };

    TypeRef (*get_parameter_type)() { nullptr };

    void* (*allocate)(const TypeRef& type) { nullptr };

    AssetStatus (*load)(AssetLoaderContext* ctx, const i32 stream_count, const TypeRef* stream_types, io::Stream** streams) { nullptr };

    AssetStatus (*unload)(AssetLoaderContext* ctx) { nullptr };
};

struct AssetLocatorInstance;

struct AssetLocator
{
    AssetLocatorInstance* instance { nullptr };

    const char* (*get_name)() { nullptr };

    bool (*locate)(AssetLocatorInstance* instance, const GUID& guid, AssetLocation* location) { nullptr };
};


#define BEE_ASSET_REGISTRY_MODULE_NAME "BEE_ASSET_REGISTRY_MODULE"

class JobGroup;

struct AssetRegistryModule
{
    void (*init)() { nullptr };

    void (*destroy)() { nullptr };

    AssetData* (*load_asset_data)(const GUID& guid, const TypeRef& type, const AssetLoadArg& arg, JobGroup* wait_handle) { nullptr };

    void (*unload_asset_data)(AssetData* asset, const UnloadAssetMode unload_kind) { nullptr };

    AssetManifest* (*add_manifest)(const StringView& name) { nullptr };

    void (*remove_manifest)(const StringView& name) { nullptr };

    AssetManifest* (*get_manifest)(const StringView& name) { nullptr };

    void (*serialize_manifests)(const SerializerMode mode, io::Stream* stream) { nullptr };

    void (*add_loader)(AssetLoader* loader) { nullptr };

    void (*remove_loader)(AssetLoader* loader) { nullptr };

    void (*add_locator)(AssetLocator* locator) { nullptr };

    void (*remove_locator)(AssetLocator* locator) { nullptr };
};


class AssetLoaderContext
{
public:
    explicit AssetLoaderContext(AssetRegistryModule* registry, AssetData* data)
        : registry_(registry),
          data_(data)
    {}

    inline TypeRef type() const
    {
        return data_->type;
    }

    inline TypeRef arg_type() const
    {
        return data_->parameter_type;
    }

    inline AssetRegistryModule* registry() const
    {
        return registry_;
    }

    template <typename T>
    inline T* get_asset()
    {
        if (BEE_FAIL_F(get_type<T>() == type(), "Invalid type cast"))
        {
            return nullptr;
        }

        return static_cast<T*>(data_->ptr);
    }

    template <typename T>
    inline T* get_arg()
    {
        if (BEE_FAIL_F(get_type<T>() == arg_type(), "Invalid type cast"))
        {
            return nullptr;
        }

        return reinterpret_cast<T*>(data_->argument_storage);
    }

private:
    AssetRegistryModule*    registry_ { nullptr };
    AssetData*              data_ { nullptr };
};

template <typename T>
class BEE_REFLECT(serializable) Asset
{
public:
    Asset() = default;

    explicit Asset(const GUID& guid)
        : guid_(guid)
    {}

    Asset(const Asset& other)
    {
        guid_ = other.guid_;
        registry_ = other.registry_;
        data_ = other.data_;

        if (data_ != nullptr)
        {
            data_->refcount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    Asset(Asset&& other) noexcept
    {
        guid_ = other.guid_;
        data_ = other.data_;
        registry_ = other.registry_;
        other.guid_ = invalid_guid;
        other.data_ = nullptr;
        other.registry_ = nullptr;
    }

    ~Asset()
    {
        unload();
    }

    Asset<T>& operator=(Asset&& other) noexcept
    {
        unload();

        guid_ = other.guid_;
        data_ = other.data_;
        registry_ = other.registry_;
        other.guid_ = invalid_guid;
        other.data_ = nullptr;
        other.registry_ = nullptr;

        return *this;
    }

    Asset<T>& operator=(const Asset& other)
    {
        if (&other == this)
        {
            return *this;
        }

        unload();
        guid_ = other.guid_;
        registry_ = other.registry_;
        data_ = other.data_;

        if (data_ != nullptr)
        {
            data_->refcount.fetch_add(1, std::memory_order_relaxed);
        }

        return *this;
    }

    inline bool load(AssetRegistryModule* registry, JobGroup* wait_handle = nullptr)
    {
        if (registry_ != nullptr && registry != registry_)
        {
            unload();
        }

        const auto type = get_type<T>();
        registry_ = registry;
        data_ = registry_->load_asset_data(guid_, type, {}, wait_handle);

        if (data_ == nullptr || data_->status == AssetStatus::unloaded)
        {
            return false;
        }

        return BEE_CHECK(data_->type == type);
    }

    template <typename ArgType>
    inline bool load(AssetRegistryModule* registry, const ArgType& arg, JobGroup* wait_handle = nullptr)
    {
        if (registry_ != nullptr && registry != registry_)
        {
            unload();
        }

        AssetLoadArg load_arg{};
        load_arg.type = get_type<ArgType>();
        load_arg.data = &arg;

        const auto type = get_type<T>();
        registry_ = registry;
        data_ = registry_->load_asset_data(guid_, type, load_arg, wait_handle);

        if (data_ == nullptr || (data_->status != AssetStatus::loading && data_->status != AssetStatus::loaded))
        {
            return false;
        }

        return BEE_CHECK(data_->type == type);
    }

    inline void unload(const UnloadAssetMode mode = UnloadAssetMode::release)
    {
        if (data_ != nullptr)
        {
            BEE_ASSERT(registry_ != nullptr);
            registry_->unload_asset_data(data_, mode);
        }

        data_ = nullptr;
        registry_ = nullptr;
    }

    inline AssetStatus status() const
    {
        return data_ == nullptr ? AssetStatus::invalid : data_->status;
    }

    inline const GUID& guid() const
    {
        return guid_;
    }

    inline T* operator->()
    {
        BEE_ASSERT(data_ != nullptr && data_->ptr != nullptr);
        return static_cast<T*>(data_->ptr);
    }

    inline const T* operator->() const
    {
        BEE_ASSERT(data_ != nullptr && data_->ptr != nullptr);
        return static_cast<const T*>(data_->ptr);
    }

    inline explicit operator bool() const
    {
        return guid_ != invalid_guid;
    }

private:
    BEE_REFLECT() GUID      guid_;

    AssetData*              data_ { nullptr };
    AssetRegistryModule*    registry_ { nullptr };
};

template <typename T, i32 Size>
inline Asset<T> AssetManifest::load(AssetRegistryModule* registry, const char(&name)[Size])
{
    const auto guid = get<Size>(name);
    if (guid == invalid_guid)
    {
        log_error("Failed to load asset \"%s\" from manifest", name);
        return Asset<T>{};
    }

    Asset<T> asset(guid);
    asset.load(registry);
    return std::move(asset);
}

template <typename T, typename ArgType, i32 Size>
inline Asset<T> AssetManifest::load(AssetRegistryModule* registry, const char(&name)[Size], const ArgType& arg)
{
    const auto guid = get<Size>(name);
    if (guid == invalid_guid)
    {
        log_error("Failed to load asset \"%s\" from manifest", name);
        return Asset<T>{};
    }

    Asset<T> asset(guid);
    asset.load(registry, arg);
    return std::move(asset);
}

/*
 **************************
 *
 * Asset<T> serialization
 *
 **************************
 */
template <typename T>
inline void serialize_type(SerializationBuilder* builder, Asset<T>* data)
{
    auto guid = data->guid();
    builder->structure(1).add_field(1, &guid, "guid");

    if (builder->mode() == SerializerMode::reading)
    {
        new (data) Asset<T>(guid);
    }
}

} // namespace bee

#ifdef BEE_ENABLE_REFLECTION
    #include "Bee.AssetRegistry/ReflectedTemplates/AssetRegistry.generated.inl"
#endif // BEE_ENABLE_REFLECTIONs