/*
 *  Shader.hpp
 *  Bee
 *
 *  Copyright (c) 2019 Jacob Milligan. All rights reserved.
 */

#pragma once


#include "Bee/Graphics/BSC.hpp"
#include "Bee/Asset/AssetSystem.hpp"
#include "Bee/Core/Serialization/StreamSerializer.hpp"


namespace bee {


struct Shader
{

};


class ShaderLoader final : public AssetLoader
{
public:
    void setup(DynamicArray<Type>* context) override
    {
        context->push_back(Type::from_static<Shader>());
    }

    bool load_asset(const AssetLoadMode mode, AssetPtr& asset, io::Stream* src_stream) override
    {
        BSCModule module;
        StreamSerializer serializer(src_stream);
        serialize(SerializerMode::reading, &serializer, &module);
        return true;
    }

    void unload_asset(const AssetUnloadMode mode, AssetPtr& asset) override
    {

    }

private:

};


} // namespace bee