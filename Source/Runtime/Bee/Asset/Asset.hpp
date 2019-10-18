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
#include "Bee/Asset/AssetCache.hpp"

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


class AssetPtr
{
public:
    AssetPtr() = default;

    AssetPtr(void* data, const Type& type)
        : data_(data),
          type_(type)
    {}

    template <typename T>
    T* as()
    {
        BEE_ASSERT(data_ != nullptr);
        BEE_ASSERT_F(Type::from_static<T>() == type_, "Tried to cast asset pointer to mismatched type");
        return static_cast<T*>(data_);
    }

    inline Type& type()
    {
        return type_;
    }

    inline const Type& type() const
    {
        return type_;
    }
private:
    void*   data_ { nullptr };
    Type    type_;
};


class BEE_RUNTIME_API AssetData
{
public:
    AssetData() = default;

    AssetData(const Type& type, const AssetHandle& handle, const i32 cache, const i32 loader);

    ~AssetData();

    void unload(const AssetUnloadMode mode = AssetUnloadMode::unload_default);

    void invalidate();

    inline bool is_valid() const
    {
        return type_.is_valid() && handle_.is_valid() && cache_ >= 0 && loader_ >= 0;
    }

    inline const Type& type() const
    {
        return type_;
    }

    inline i32 loader() const
    {
        return loader_;
    }

    inline i32 cache() const
    {
        return cache_;
    }

    inline const AssetHandle& handle() const
    {
        return handle_;
    }
protected:
    i32             cache_ { -1 };
    i32             loader_ { -1 };
    Type            type_;
    AssetHandle     handle_;
};


template <typename T>
class Asset final : public AssetData
{
public:
    Asset()
        : AssetData(Type::from_static<T>(), AssetHandle(), -1, -1)
    {}

    explicit Asset(const AssetData& data)
        : AssetData(data)
    {}

    T* get()
    {
        BEE_ASSERT(loader_ != nullptr);
        BEE_ASSERT(handle_.is_valid());
        return static_cast<T*>(cache_->get(type_, handle_));
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





} // namespace bee