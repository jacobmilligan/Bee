/*
 *  AssetCache.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once

#include "Bee/Core/Handle.hpp"
#include "Bee/Core/Reflection.hpp"
#include "Bee/Core/Containers/HashMap.hpp"
#include "Bee/Core/Containers/ResourcePool.hpp"

namespace bee {


BEE_VERSIONED_HANDLE_32(AssetHandle);


struct BEE_RUNTIME_API AssetCache
{
    virtual void setup(DynamicArray<Type>* supported_types) = 0;

    virtual AssetHandle allocate(const Type& type) = 0;

    virtual void deallocate(const Type& type, const AssetHandle& handle) = 0;

    virtual void* get(const Type& type, const AssetHandle& handle) = 0;

    virtual void trim() = 0;
};


class BEE_RUNTIME_API DefaultAssetCache final : public AssetCache
{
public:
    DefaultAssetCache();

    void setup(DynamicArray<Type>* supported_types) override;

    AssetHandle allocate(const Type& type) override;

    void deallocate(const Type& type, const AssetHandle& handle) override;

    void* get(const Type& type, const AssetHandle& handle) override;

    void trim() override;

private:
    ResourcePool<AssetHandle, void*> assets_;
};


} // namespace bee