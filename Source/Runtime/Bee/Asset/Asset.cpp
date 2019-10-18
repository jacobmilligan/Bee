/*
 *  Asset.cpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#include "Bee/Asset/AssetSystem.hpp"


namespace bee {


AssetData::AssetData(const Type& type, const AssetHandle& handle, const i32 cache, const i32 loader)
    : type_(type),
      handle_(handle),
      cache_(cache),
      loader_(loader)
{}

AssetData::~AssetData()
{
    invalidate();
}

void AssetData::unload(const AssetUnloadMode mode)
{
    BEE_ASSERT(cache_ >= 0);
    BEE_ASSERT(loader_ >= 0);
    BEE_ASSERT(handle_.is_valid());
    unload_asset(this, mode);
    handle_ = AssetHandle();
}

void AssetData::invalidate()
{
    if (is_valid())
    {
        unload();
    }

    loader_ = -1;
    cache_ = -1;
    handle_ = AssetHandle();
}


} // namespace bee