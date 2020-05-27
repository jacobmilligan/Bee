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

enum class AssetLocationKind
{
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

struct AssetLoadArg
{
    const Type* type { get_type<UnknownType>() };
    const void* data { nullptr };
};

struct AssetData
{
    static constexpr i32 load_parameter_capacity = 128;

    GUID                    guid;
    AssetId                 id;
    AssetStatus             status { AssetStatus::invalid };
    AssetLoader*            loader { nullptr };
    std::atomic_int32_t     refcount { 0 };
    const Type*             type { nullptr };
    void*                   ptr { nullptr };
    const Type*             parameter_type { nullptr };
    u8                      parameter_storage[load_parameter_capacity];
};


struct AssetLocation
{
    const Type* type { nullptr };
    Path        path;
    size_t      offset { 0 };
};

struct AssetLoader
{
    void (*get_supported_types)(DynamicArray<const Type*>* types) { nullptr };

    const Type* (*get_parameter_type)() { nullptr };

    void* (*allocate)(const Type* type) { nullptr };

    AssetStatus (*load)(AssetLoaderContext* ctx, io::Stream* stream) { nullptr };

    AssetStatus (*unload)(AssetLoaderContext* ctx) { nullptr };
};

struct AssetLocator
{
    bool (*locate)(const GUID& guid, AssetLocation* location) { nullptr };
};


#define BEE_ASSET_REGISTRY_MODULE_NAME "BEE_ASSET_REGISTRY_MODULE"

struct AssetRegistryModule
{
    void (*init)() { nullptr };

    void (*destroy)() { nullptr };

    AssetData* (*load_asset_data)(const GUID& guid, const Type* type, const AssetLoadArg& arg) { nullptr };

    void (*unload_asset_data)(AssetData* asset, const UnloadAssetMode unload_kind) { nullptr };

    bool (*find_guid)(GUID* dst, const StringView& name, const Type* type) { nullptr };

    void (*add_loader)(AssetLoader* loader) { nullptr };

    void (*remove_loader)(AssetLoader* loader) { nullptr };

    void (*add_locator)(AssetLocator* locator) { nullptr };

    void (*remove_locator)(AssetLocator* locator) { nullptr };

    template <typename T>
    inline Asset<T> load_asset(const GUID& guid)
    {
        return Asset<T>(load_asset_data(guid, get_type<T>(), {}));
    }

    template <typename T, typename ArgType>
    inline Asset<T> load_asset(const GUID& guid, const T& arg)
    {
        AssetLoadArg load_arg{};
        load_arg.type = get_type<ArgType>();
        load_arg.data = &arg;
        return Asset<T>(load_asset_data(guid, get_type<T>(), load_arg), this);
    }

    template <typename T>
    inline Asset<T> load_asset(const StringView& name)
    {
        GUID guid{};
        if (!find_guid(&guid, name, get_type<T>()))
        {
            return Asset<T>(nullptr, this);
        }

        return Asset<T>(load_asset_data(guid, get_type<T>(), {}), this);
    }

    template <typename T, typename ArgType>
    inline Asset<T> load_asset(const StringView& name, const T& arg)
    {
        AssetLoadArg load_arg{};
        load_arg.type = get_type<ArgType>();
        load_arg.data = &arg;

        GUID guid{};
        if (!find_guid(&guid, name, get_type<T>()))
        {
            return Asset<T>(nullptr, this);
        }

        return Asset<T>(load_asset_data(guid, get_type<T>(), load_arg), this);
    }
};


class AssetLoaderContext
{
public:
    explicit AssetLoaderContext(AssetData* data)
        : data_(data)
    {}

    inline const Type* type() const
    {
        return data_->type;
    }

    inline const Type* parameter_type() const
    {
        return data_->parameter_type;
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
    inline T* get_parameter()
    {
        if (BEE_FAIL_F(get_type<T>() == parameter_type(), "Invalid type cast"))
        {
            return nullptr;
        }

        return reinterpret_cast<T*>(data_->parameter_storage);
    }

private:
    AssetData*          data_ { nullptr };
};

template <typename T>
class Asset
{
public:
    Asset() = default;

    Asset(AssetData* data, AssetRegistryModule* registry)
        : data_(data),
          registry_(registry)
    {
        BEE_ASSERT(data->type == get_type<T>());
        ptr_ = static_cast<T*>(data->ptr);
    }

    ~Asset()
    {
        unload();
    }

    inline void unload(const UnloadAssetMode mode = UnloadAssetMode::release)
    {
        if (ptr_ != nullptr)
        {
            registry_->unload_asset_data(data_, mode);
        }

        ptr_ = nullptr;
        data_ = nullptr;
        registry_ = nullptr;
    }

    inline AssetStatus status() const
    {
        return ptr_ == nullptr ? AssetStatus::invalid : data_->status;
    }

    inline T* operator->()
    {
        BEE_ASSERT(ptr_ != nullptr);
        return ptr_
    }

    inline const T* operator->() const
    {
        BEE_ASSERT(ptr_ != nullptr);
        return ptr_;
    }

private:
    T*                      ptr_ { nullptr };
    AssetData*              data_ { nullptr };
    AssetRegistryModule*    registry_ { nullptr };
};


} // namespace bee