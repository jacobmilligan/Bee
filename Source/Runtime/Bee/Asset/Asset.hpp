/*
 *  Asset.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/IO.hpp"
#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/Memory/SmartPointers.hpp"
#include "Bee/Core/Concurrency.hpp"
#include "Bee/Core/GUID.hpp"
#include "Bee/Core/Jobs/JobSystem.hpp"

namespace bee {


enum class AssetUnloadMode
{
    unload_default,
    unload_immediate
};

enum class AssetLoadMode
{
    load,
    reload
};


BEE_DEFINE_VERSIONED_HANDLE(Asset);


struct AssetLoaderSetupContext
{
    DynamicArray<Type> supported_types;
};


struct BEE_RUNTIME_API AssetLoader
{
    virtual ~AssetLoader() = default;

    virtual void setup(AssetLoaderSetupContext* context) = 0;

    virtual AssetHandle allocate_asset(const Type& asset_type, Allocator* default_allocator) = 0;

    virtual bool load_asset(const AssetHandle& handle, const AssetLoadMode mode, const Type& asset_type, io::Stream* src_stream, Allocator* default_allocator) = 0;

    virtual void unload_asset(const AssetHandle& handle, const AssetUnloadMode mode, const Type& asset_type) = 0;

    virtual void* get_asset_data(const bee::AssetHandle& handle, const bee::Type& asset_type) = 0;

    template <typename T>
    T* get_asset_data(const AssetHandle& asset)
    {
        return static_cast<T*>(get_asset_data(asset, Type::from_static<T>()));
    }
};


struct BEE_RUNTIME_API AssetRegistry
{
    virtual ~AssetRegistry() = default;

    virtual bool locate_asset(const GUID& guid, io::FileStream* dst_stream) = 0;
};


class AssetSystem;


class BEE_RUNTIME_API AssetData
{
public:
    AssetData() = default;

    AssetData(AssetSystem* owner, const Type& type, i32 loader, const AssetHandle& handle);

    ~AssetData();

    void unload(const AssetUnloadMode mode = AssetUnloadMode::unload_default);

    void invalidate();

    inline bool is_valid() const
    {
        return loader_ >= 0 && handle_.is_valid();
    }

    inline const Type& type() const
    {
        return type_;
    }

    inline i32 loader() const
    {
        return loader_;
    }

    inline const AssetHandle& handle() const
    {
        return handle_;
    }

    AssetSystem* owner()
    {
        return owner_;
    }
protected:
    AssetSystem*    owner_ { nullptr };
    Type            type_;
    i32             loader_ { -1 };
    AssetHandle     handle_;
};


template <typename T>
class Asset final : public AssetData
{
public:
    Asset()
        : AssetData(Type::from_static<T>(), nullptr, AssetHandle())
    {}

    T* get()
    {
        BEE_ASSERT(loader_ != nullptr);
        BEE_ASSERT(handle_.is_valid());
        return loader_->get_asset_data<T>(handle_);
    }

    inline T* operator->()
    {
        return get();
    }

    inline const T* operator->() const
    {
        return get();
    }

    inline T& operator*()
    {
        return *get();
    }
};


struct AssetLoadRequest
{
    GUID            asset_guid;
    AssetLoadMode   mode;
};


class BEE_RUNTIME_API AssetSystem
{
public:
    template <typename LoaderType>
    void register_loader(LoaderType* loader)
    {
        register_loader(Type::from_static<LoaderType>(), loader);
    }

    template <typename LoaderType>
    void unregister_loader()
    {
        unregister_loader(Type::from_static<LoaderType>());
    }

    template <typename RegistryType>
    void add_registry(RegistryType* registry)
    {
        add_registry(Type::from_static<RegistryType>(), registry);
    }

    template <typename RegistryType>
    void remove_registry()
    {
        remove_registry(Type::from_static<RegistryType>());
    }

    void load_assets(JobGroup* group, const AssetLoadRequest* load_requests, AssetData* dst_assets, const i32 count);

    void unload_asset(AssetData* asset, const AssetUnloadMode mode);
private:
    struct LoaderInfo
    {
        Type                type;
        ReaderWriterMutex   mutex;
        AssetLoader*        loader { nullptr };
        DynamicArray<Type>  supported_types;

        LoaderInfo(const Type& new_type, AssetLoader* new_loader)
            : type(new_type),
              loader(new_loader)
        {}
    };

    struct RegistryInfo
    {
        Type                        type;
        AssetRegistry*              registry { nullptr };

        RegistryInfo(const Type& new_type, AssetRegistry* new_registry)
            : type(new_type),
              registry(new_registry)
        {}
    };

    struct LoadAssetJob final : public Job
    {
        AssetSystem*                    asset_system { nullptr };
        FixedArray<AssetLoadRequest>    requests;
        AssetData*                      assets { nullptr };

        LoadAssetJob(AssetSystem* system, const AssetLoadRequest* load_requests, AssetData* dst_assets, const i32 request_count);

        void execute() override;
    };

    ReaderWriterMutex                   loader_mutex_;
    DynamicArray<LoaderInfo>            loader_infos_;

    SpinLock                            registry_mutex_;
    DynamicArray<RegistryInfo>          registry_infos_;

    SpinLock                            asset_cache_mutex_;
    DynamicHashMap<GUID, AssetData>     asset_cache_;

    inline i32 find_loader_no_lock(const Type& type)
    {
        return container_index_of(loader_infos_, [&](const LoaderInfo& l) { return l.type == type; } );
    }

    inline i32 find_registry_no_lock(const Type& type)
    {
        return container_index_of(registry_infos_, [&](const RegistryInfo& r) { return r.type == type; } );
    }

    void register_loader(const Type& type, AssetLoader* loader);

    void unregister_loader(const Type& type);

    void add_registry(const Type& type, AssetRegistry* registry);

    void remove_registry(const Type& type);

    bool locate_asset(const GUID& guid, io::FileStream* stream);

    AssetData* find_cached_asset(const GUID& guid);

    i32 find_loader_for_type_no_lock(const Type& type);

};


} // namespace bee