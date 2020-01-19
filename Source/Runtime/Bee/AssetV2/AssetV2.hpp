/*
 *  Asset.hpp
 *  Bee
 *
 *  Copyright (c) 2020 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/Handle.hpp"
#include "Bee/Core/GUID.hpp"
#include "Bee/Core/IO.hpp"


namespace bee {


enum class AssetStatus
{
    invalid,
    unloaded,
    loading,
    loading_failed,
    loaded
};


enum class AssetUnloadType
{
    release,
    destroy
};

enum class AssetLocationType
{
    invalid,
    file,
    in_memory
};


BEE_VERSIONED_HANDLE_32(AssetHandle);


struct AssetInfo
{
    GUID        guid;
    const Type* type { nullptr };
    AssetStatus status { AssetStatus::invalid };
    AssetHandle handle;
    i32         loader { -1 };
};

class AssetData
{
public:
    AssetData() = default;

    AssetData(const Type* type, void* data)
        : type_(type),
          data_(data)
    {}

    template <typename T>
    inline T* as()
    {
        BEE_ASSERT(::bee::get_type<T>() == type_);
        return static_cast<T*>(data_);
    }

    inline const Type* type() const
    {
        BEE_ASSERT(type_ != nullptr);
        return type_;
    }

    void* data()
    {
        BEE_ASSERT(data_ != nullptr);
        return data_;
    }
private:
    const Type* type_ { nullptr };
    void*       data_ { nullptr };
};


template <typename T>
class Asset
{
public:
    Asset() = default;

    Asset(AssetInfo* info, T* data)
        : info_(info),
          data_(data)
    {}

    ~Asset();

    void unload(const AssetUnloadType& type = AssetUnloadType::release);

    inline AssetStatus status() const
    {
        BEE_ASSERT(info_ != nullptr);
        return info_->status;
    }

    inline T* data()
    {
        BEE_ASSERT(data_ != nullptr);
        return data_;
    }

    inline const T* data() const
    {
        BEE_ASSERT(data_ != nullptr);
        return data_;
    }

    inline const GUID& guid() const
    {
        BEE_ASSERT(info_ != nullptr);
        return info_->guid;
    }

    inline T* operator->()
    {
        return data();
    }

    inline const T* operator->() const
    {
        return data();
    }

    inline T& operator*()
    {
        return *data();
    }

    inline const T& operator*() const
    {
        return *data();
    }

private:
    AssetInfo*  info_ { nullptr };
    T*          data_ { nullptr };
};

struct BEE_RUNTIME_API AssetLoader
{
    virtual ~AssetLoader() = default;
    virtual AssetHandle allocate(const Type* type) = 0;
    virtual AssetData get(const Type* type, const AssetHandle& handle) = 0;
    virtual AssetStatus load(AssetData* dst_data, io::Stream* src_stream) = 0;
    virtual AssetStatus unload(AssetData* data, const AssetUnloadType unload_type) = 0;
};

struct BEE_RUNTIME_API AssetLocation
{
    AssetLocationType   type { AssetLocationType::invalid };
    const char*         file_path { nullptr };
    const void*         read_only_buffer { nullptr };
    i32                 read_only_buffer_size { 0 };
};

struct BEE_RUNTIME_API AssetLocator
{
    virtual ~AssetLocator() = default;
    virtual bool locate(const GUID& guid, AssetLocation* location) = 0;
};

BEE_RUNTIME_API void assets_init();

BEE_RUNTIME_API void assets_shutdown();

BEE_RUNTIME_API void register_asset_name(const char* name, const GUID& guid);

BEE_RUNTIME_API void unregister_asset_name(const char* name);

BEE_RUNTIME_API void register_asset_loader(const char* name, AssetLoader* loader, std::initializer_list<const Type*> supported_types);

BEE_RUNTIME_API void unregister_asset_loader(const char* name);

BEE_RUNTIME_API void register_asset_locator(const char* name, AssetLocator* locator);

BEE_RUNTIME_API void unregister_asset_locator(const char* name);

BEE_RUNTIME_API bool request_asset_load(const GUID& guid, const Type* requested_type, AssetInfo** info, AssetData* data);

BEE_RUNTIME_API bool asset_name_to_guid(const char* name, GUID* dst_guid);

BEE_RUNTIME_API void unload_asset(AssetInfo* info, const AssetUnloadType unload_type);

template <typename AssetType>
inline Asset<AssetType> load_asset(const GUID& guid)
{
    AssetInfo* info = nullptr;
    AssetData data{};
    if (BEE_FAIL_F(request_asset_load(guid, get_type<AssetType>(), &info, &data), "Failed to load asset %s", guid_to_string(guid, GUIDFormat::digits, temp_allocator()).c_str()))
    {
        return Asset<AssetType>{};
    }

    BEE_ASSERT(info != nullptr);
    BEE_ASSERT(data.data() != nullptr);
    return Asset<AssetType>(info, data.as<AssetType>());
}

template <typename AssetType>
inline Asset<AssetType> load_asset(const char* name)
{
    GUID guid{};

    if (BEE_FAIL_F(asset_name_to_guid(name, &guid), "Failed to load asset: no GUID found for asset name: %s", name))
    {
        return Asset<AssetType>{};
    }

    return load_asset<AssetType>(guid);
}

template <typename T>
void unload_asset(const Asset<T>& asset)
{
    unload_asset(asset.handle());
}

template <typename T>
Asset<T>::~Asset()
{
    unload(AssetUnloadType::release);
}

template <typename T>
void Asset<T>::unload(const AssetUnloadType& type)
{
    if (info_ != nullptr)
    {
        unload_asset(info_, type);
    }

    data_ = nullptr;
    info_ = nullptr;
}


} // namespace bee