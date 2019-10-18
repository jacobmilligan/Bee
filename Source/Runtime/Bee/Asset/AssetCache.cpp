/*
 *  AssetCache.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Asset/AssetCache.hpp"

namespace bee {


DefaultAssetCache::DefaultAssetCache()
    : assets_(sizeof(void*) * 64)
{}

void DefaultAssetCache::setup(DynamicArray<Type>* /* supported_types */)
{
    // no-op: this cache is only used by the asset system as a fallback when no other cache is available
}

AssetHandle DefaultAssetCache::allocate(const Type& type)
{
    const auto handle =  assets_.allocate();
    assets_[handle] = BEE_MALLOC_ALIGNED(system_allocator(), type.size, type.alignment);
    return handle;
}

void DefaultAssetCache::deallocate(const Type& type, const AssetHandle& handle)
{
    auto& asset = assets_[handle];
    BEE_FREE(system_allocator(), asset);
    assets_.deallocate(handle);
}

void* DefaultAssetCache::get(const Type& type, const AssetHandle& handle)
{
    return assets_[handle];
}

void DefaultAssetCache::trim()
{
    assets_.shrink_to_fit();
}


} // namespace bee