/*
 *  Shader.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Graphics/BSC.hpp"


namespace bee {


struct Shader
{

};


class ShaderLoader final : public AssetLoader
{
public:
    void setup(AssetLoaderSetupContext* context) override
    {
        context->supported_types.push_back(Type::from_static<Shader>());
    }

    AssetHandle allocate_asset(const Type& asset_type, Allocator* default_allocator) override
    {
        return AssetHandle();
    }

    bool load_asset(const AssetHandle& handle, const AssetLoadMode mode, const Type& asset_type, io::Stream* src_stream,
                    Allocator* default_allocator) override
    {
        return false;
    }

    void unload_asset(const AssetHandle& handle, const AssetUnloadMode mode, const Type& asset_type) override
    {

    }

    void* get_asset_data(const bee::AssetHandle& handle, const bee::Type& asset_type) override
    {
        return nullptr;
    }

private:

};


} // namespace bee